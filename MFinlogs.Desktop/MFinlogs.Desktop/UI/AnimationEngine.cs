namespace MFinlogs.Desktop.UI;

/// <summary>
/// High-performance animation engine with spring physics, easing functions,
/// and timeline management. Targets 60fps with minimal allocations.
/// Inspired by Framer Motion, React Spring, and Apple's Core Animation.
/// </summary>
public class AnimationEngine : IDisposable
{
    private readonly System.Windows.Forms.Timer _timer;
    private readonly List<IAnimation> _animations = new();
    private readonly List<IAnimation> _pendingAdd = new();
    private readonly List<IAnimation> _pendingRemove = new();
    private bool _disposed;
    private bool _isUpdating;

    public AnimationEngine()
    {
        _timer = new System.Windows.Forms.Timer
        {
            Interval = DesignTokens.Motion.FrameInterval
        };
        _timer.Tick += OnTick;
    }

    /// <summary>Start the animation loop</summary>
    public void Start() => _timer.Start();

    /// <summary>Stop the animation loop</summary>
    public void Stop() => _timer.Stop();

    /// <summary>Add an animation to the engine</summary>
    public void Add(IAnimation anim)
    {
        if (_isUpdating)
            _pendingAdd.Add(anim);
        else
            _animations.Add(anim);

        if (!_timer.Enabled) _timer.Start();
    }

    /// <summary>Remove an animation from the engine</summary>
    public void Remove(IAnimation anim)
    {
        if (_isUpdating)
            _pendingRemove.Add(anim);
        else
            _animations.Remove(anim);
    }


    private void OnTick(object? sender, EventArgs e)
    {
        _isUpdating = true;

        for (int i = _animations.Count - 1; i >= 0; i--)
        {
            var anim = _animations[i];
            anim.Update();

            if (anim.IsComplete)
            {
                anim.OnComplete?.Invoke();
                _animations.RemoveAt(i);
            }
        }

        _isUpdating = false;

        // Process pending additions/removals
        if (_pendingAdd.Count > 0)
        {
            _animations.AddRange(_pendingAdd);
            _pendingAdd.Clear();
        }
        if (_pendingRemove.Count > 0)
        {
            foreach (var r in _pendingRemove)
                _animations.Remove(r);
            _pendingRemove.Clear();
        }

        // Auto-stop when no animations
        if (_animations.Count == 0)
            _timer.Stop();
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            _timer.Stop();
            _timer.Dispose();
            _animations.Clear();
            _disposed = true;
        }
    }
}


/// <summary>Base animation interface</summary>
public interface IAnimation
{
    bool IsComplete { get; }
    void Update();
    Action? OnComplete { get; set; }
}

/// <summary>
/// Tween animation — animates a float from start to end using an easing function.
/// </summary>
public class TweenAnimation : IAnimation
{
    private float _elapsed;
    private readonly float _duration;
    private readonly float _from;
    private readonly float _to;
    private readonly Func<float, float> _easing;
    private readonly Action<float> _onUpdate;

    public bool IsComplete => _elapsed >= _duration;
    public Action? OnComplete { get; set; }
    public float CurrentValue { get; private set; }

    public TweenAnimation(
        float from, float to, float durationMs,
        Func<float, float> easing, Action<float> onUpdate)
    {
        _from = from;
        _to = to;
        _duration = durationMs;
        _easing = easing;
        _onUpdate = onUpdate;
        CurrentValue = from;
    }

    public void Update()
    {
        _elapsed += DesignTokens.Motion.FrameInterval;
        float t = Math.Min(_elapsed / _duration, 1f);
        float easedT = _easing(t);
        CurrentValue = _from + (_to - _from) * easedT;
        _onUpdate(CurrentValue);
    }
}


/// <summary>
/// Spring animation — physics-based motion with damping and stiffness.
/// Feels natural and responsive like Apple/Linear animations.
/// </summary>
public class SpringAnimation : IAnimation
{
    private float _velocity;
    private float _position;
    private readonly float _target;
    private readonly float _damping;
    private readonly float _stiffness;
    private readonly Action<float> _onUpdate;
    private const float Threshold = 0.001f;
    private const float VelocityThreshold = 0.001f;

    public bool IsComplete { get; private set; }
    public Action? OnComplete { get; set; }
    public float CurrentValue => _position;

    public SpringAnimation(
        float from, float to,
        float damping, float stiffness,
        Action<float> onUpdate)
    {
        _position = from;
        _target = to;
        _damping = damping;
        _stiffness = stiffness;
        _onUpdate = onUpdate;
        _velocity = 0f;
    }

    public void Update()
    {
        float displacement = _position - _target;
        float springForce = -_stiffness * displacement;
        float dampingForce = -_damping * _velocity;
        float acceleration = springForce + dampingForce;

        _velocity += acceleration;
        _position += _velocity;

        _onUpdate(_position);

        // Check if settled
        if (MathF.Abs(_velocity) < VelocityThreshold &&
            MathF.Abs(_position - _target) < Threshold)
        {
            _position = _target;
            _onUpdate(_position);
            IsComplete = true;
        }
    }
}


/// <summary>
/// Delayed animation — waits before executing
/// </summary>
public class DelayedAnimation : IAnimation
{
    private float _elapsed;
    private readonly float _delay;
    private readonly IAnimation _inner;
    private bool _started;

    public bool IsComplete => _started && _inner.IsComplete;
    public Action? OnComplete { get; set; }

    public DelayedAnimation(float delayMs, IAnimation inner)
    {
        _delay = delayMs;
        _inner = inner;
    }

    public void Update()
    {
        if (!_started)
        {
            _elapsed += DesignTokens.Motion.FrameInterval;
            if (_elapsed >= _delay)
                _started = true;
            return;
        }
        _inner.Update();
    }
}

/// <summary>
/// Sequence animation — plays animations one after another
/// </summary>
public class SequenceAnimation : IAnimation
{
    private readonly IAnimation[] _sequence;
    private int _currentIndex;

    public bool IsComplete => _currentIndex >= _sequence.Length;
    public Action? OnComplete { get; set; }

    public SequenceAnimation(params IAnimation[] animations)
    {
        _sequence = animations;
    }

    public void Update()
    {
        if (IsComplete) return;

        var current = _sequence[_currentIndex];
        current.Update();

        if (current.IsComplete)
        {
            current.OnComplete?.Invoke();
            _currentIndex++;
        }
    }
}

/// <summary>
/// Looping animation — repeats a value between 0 and 1 continuously
/// </summary>
public class LoopAnimation : IAnimation
{
    private float _elapsed;
    private readonly float _duration;
    private readonly Action<float> _onUpdate;
    private readonly Func<float, float> _easing;

    public bool IsComplete => false; // Never completes
    public Action? OnComplete { get; set; }

    public LoopAnimation(float durationMs, Action<float> onUpdate,
        Func<float, float>? easing = null)
    {
        _duration = durationMs;
        _onUpdate = onUpdate;
        _easing = easing ?? DesignTokens.Motion.Linear;
    }

    public void Update()
    {
        _elapsed += DesignTokens.Motion.FrameInterval;
        if (_elapsed >= _duration) _elapsed -= _duration;
        float t = _elapsed / _duration;
        _onUpdate(_easing(t));
    }
}
