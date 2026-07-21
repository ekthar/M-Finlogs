using System.Drawing.Text;

namespace MFinlogs.Desktop.UI;

/// <summary>
/// M-Finlogs Design System — Complete token library.
/// Inspired by Linear, Arc Browser, Raycast, Notion, Figma Desktop.
/// Every visual decision references these tokens.
/// </summary>
public static class DesignTokens
{
    // ═══════════════════════════════════════════════════════════════
    // COLOR SYSTEM — Light Mode (Dark mode architecture prepared)
    // ═══════════════════════════════════════════════════════════════

    public static class Colors
    {
        // ─── Primary (Indigo-Violet gradient spectrum) ───
        public static readonly Color Primary50 = Color.FromArgb(238, 237, 255);
        public static readonly Color Primary100 = Color.FromArgb(218, 215, 255);
        public static readonly Color Primary200 = Color.FromArgb(183, 178, 255);
        public static readonly Color Primary300 = Color.FromArgb(141, 134, 255);
        public static readonly Color Primary400 = Color.FromArgb(109, 99, 255);
        public static readonly Color Primary500 = Color.FromArgb(88, 77, 246);
        public static readonly Color Primary600 = Color.FromArgb(73, 62, 220);
        public static readonly Color Primary700 = Color.FromArgb(60, 50, 186);
        public static readonly Color Primary800 = Color.FromArgb(49, 42, 150);
        public static readonly Color Primary900 = Color.FromArgb(38, 33, 112);

        // ─── Secondary (Cool Slate) ───
        public static readonly Color Secondary50 = Color.FromArgb(248, 250, 252);
        public static readonly Color Secondary100 = Color.FromArgb(241, 245, 249);
        public static readonly Color Secondary200 = Color.FromArgb(226, 232, 240);
        public static readonly Color Secondary300 = Color.FromArgb(203, 213, 225);
        public static readonly Color Secondary400 = Color.FromArgb(148, 163, 184);
        public static readonly Color Secondary500 = Color.FromArgb(100, 116, 139);
        public static readonly Color Secondary600 = Color.FromArgb(71, 85, 105);
        public static readonly Color Secondary700 = Color.FromArgb(51, 65, 85);
        public static readonly Color Secondary800 = Color.FromArgb(30, 41, 59);
        public static readonly Color Secondary900 = Color.FromArgb(15, 23, 42);
    }


        // ─── Accent (Warm Amber for highlights & CTAs) ───
        public static readonly Color Accent50 = Color.FromArgb(255, 251, 235);
        public static readonly Color Accent100 = Color.FromArgb(254, 243, 199);
        public static readonly Color Accent200 = Color.FromArgb(253, 230, 138);
        public static readonly Color Accent300 = Color.FromArgb(252, 211, 77);
        public static readonly Color Accent400 = Color.FromArgb(251, 191, 36);
        public static readonly Color Accent500 = Color.FromArgb(245, 158, 11);

        // ─── Semantic Colors ───
        public static readonly Color Success = Color.FromArgb(34, 197, 94);
        public static readonly Color SuccessLight = Color.FromArgb(220, 252, 231);
        public static readonly Color SuccessDark = Color.FromArgb(22, 163, 74);

        public static readonly Color Warning = Color.FromArgb(251, 146, 60);
        public static readonly Color WarningLight = Color.FromArgb(255, 237, 213);
        public static readonly Color WarningDark = Color.FromArgb(234, 88, 12);

        public static readonly Color Danger = Color.FromArgb(239, 68, 68);
        public static readonly Color DangerLight = Color.FromArgb(254, 226, 226);
        public static readonly Color DangerDark = Color.FromArgb(185, 28, 28);

        public static readonly Color Info = Color.FromArgb(59, 130, 246);
        public static readonly Color InfoLight = Color.FromArgb(219, 234, 254);
        public static readonly Color InfoDark = Color.FromArgb(29, 78, 216);

        // ─── Neutral (UI Chrome) ───
        public static readonly Color Neutral0 = Color.FromArgb(255, 255, 255);
        public static readonly Color Neutral50 = Color.FromArgb(250, 250, 252);
        public static readonly Color Neutral100 = Color.FromArgb(244, 244, 248);
        public static readonly Color Neutral200 = Color.FromArgb(233, 234, 240);
        public static readonly Color Neutral300 = Color.FromArgb(212, 214, 224);
        public static readonly Color Neutral400 = Color.FromArgb(163, 167, 183);
        public static readonly Color Neutral500 = Color.FromArgb(113, 118, 140);
        public static readonly Color Neutral600 = Color.FromArgb(82, 87, 109);
        public static readonly Color Neutral700 = Color.FromArgb(55, 59, 80);
        public static readonly Color Neutral800 = Color.FromArgb(32, 35, 52);
        public static readonly Color Neutral900 = Color.FromArgb(17, 19, 33);
        public static readonly Color Neutral950 = Color.FromArgb(8, 9, 18);


        // ─── Surface System (Layered depth) ───
        public static readonly Color Background = Color.FromArgb(251, 251, 253);
        public static readonly Color Surface = Color.FromArgb(255, 255, 255);
        public static readonly Color SurfaceElevated = Color.FromArgb(255, 255, 255);
        public static readonly Color SurfaceOverlay = Color.FromArgb(40, 0, 0, 0);
        public static readonly Color SurfaceGlass = Color.FromArgb(200, 255, 255, 255);
        public static readonly Color SurfaceDimmed = Color.FromArgb(248, 249, 252);

        // ─── Stroke / Border ───
        public static readonly Color Stroke = Color.FromArgb(233, 234, 240);
        public static readonly Color StrokeLight = Color.FromArgb(244, 244, 248);
        public static readonly Color StrokeFocus = Color.FromArgb(88, 77, 246);
        public static readonly Color StrokeHover = Color.FromArgb(203, 213, 225);
        public static readonly Color StrokeSubtle = Color.FromArgb(20, 0, 0, 0);

        // ─── Interactive States ───
        public static readonly Color Hover = Color.FromArgb(8, 0, 0, 0);
        public static readonly Color Pressed = Color.FromArgb(16, 0, 0, 0);
        public static readonly Color Disabled = Color.FromArgb(244, 244, 248);
        public static readonly Color DisabledText = Color.FromArgb(163, 167, 183);
        public static readonly Color FocusRing = Color.FromArgb(80, 88, 77, 246);
        public static readonly Color Glow = Color.FromArgb(40, 88, 77, 246);
        public static readonly Color GlowStrong = Color.FromArgb(80, 88, 77, 246);

        // ─── Text ───
        public static readonly Color TextPrimary = Color.FromArgb(17, 19, 33);
        public static readonly Color TextSecondary = Color.FromArgb(82, 87, 109);
        public static readonly Color TextTertiary = Color.FromArgb(133, 138, 160);
        public static readonly Color TextMuted = Color.FromArgb(163, 167, 183);
        public static readonly Color TextInverse = Color.FromArgb(255, 255, 255);
        public static readonly Color TextLink = Color.FromArgb(88, 77, 246);
        public static readonly Color TextOnPrimary = Color.FromArgb(255, 255, 255);


        // ─── Dark Mode Splash / Overlay Palette ───
        public static readonly Color DarkBg = Color.FromArgb(8, 9, 18);
        public static readonly Color DarkBgElevated = Color.FromArgb(17, 19, 33);
        public static readonly Color DarkSurface = Color.FromArgb(24, 26, 44);
        public static readonly Color DarkSurfaceElevated = Color.FromArgb(32, 35, 56);
        public static readonly Color DarkStroke = Color.FromArgb(45, 48, 72);
        public static readonly Color DarkTextPrimary = Color.FromArgb(240, 241, 248);
        public static readonly Color DarkTextSecondary = Color.FromArgb(163, 167, 192);
        public static readonly Color DarkTextMuted = Color.FromArgb(100, 105, 135);
        public static readonly Color DarkGlow = Color.FromArgb(60, 109, 99, 255);

        // ─── Gradient Presets ───
        public static readonly Color GradientPrimaryStart = Color.FromArgb(88, 77, 246);
        public static readonly Color GradientPrimaryEnd = Color.FromArgb(139, 92, 246);
        public static readonly Color GradientAuroraStart = Color.FromArgb(88, 77, 246);
        public static readonly Color GradientAuroraMid = Color.FromArgb(139, 92, 246);
        public static readonly Color GradientAuroraEnd = Color.FromArgb(236, 72, 153);
        public static readonly Color GradientWarmStart = Color.FromArgb(251, 146, 60);
        public static readonly Color GradientWarmEnd = Color.FromArgb(236, 72, 153);
        public static readonly Color GradientCoolStart = Color.FromArgb(59, 130, 246);
        public static readonly Color GradientCoolEnd = Color.FromArgb(88, 77, 246);
        public static readonly Color GradientSplashTop = Color.FromArgb(6, 7, 14);
        public static readonly Color GradientSplashBottom = Color.FromArgb(14, 15, 30);
    }


    // ═══════════════════════════════════════════════════════════════
    // TYPOGRAPHY — Inter Tight with comprehensive hierarchy
    // ═══════════════════════════════════════════════════════════════

    public static class Typography
    {
        // Font family management
        private static PrivateFontCollection? _pfc;
        private static FontFamily? _boldFamily;
        private static FontFamily? _regularFamily;
        private static bool _fontsLoaded;

        /// <summary>
        /// Load Inter Tight fonts from Assets directory.
        /// Call once at startup. Falls back to Segoe UI gracefully.
        /// </summary>
        public static void Initialize()
        {
            if (_fontsLoaded) return;
            _fontsLoaded = true;

            try
            {
                _pfc = new PrivateFontCollection();
                var assetsDir = Path.Combine(AppContext.BaseDirectory, "Assets");

                var boldPath = Path.Combine(assetsDir, "InterTight-Bold.ttf");
                var regularPath = Path.Combine(assetsDir, "InterTight-Regular.ttf");

                if (File.Exists(boldPath)) _pfc.AddFontFile(boldPath);
                if (File.Exists(regularPath)) _pfc.AddFontFile(regularPath);

                if (_pfc.Families.Length >= 2)
                {
                    _boldFamily = _pfc.Families[0];
                    _regularFamily = _pfc.Families[1];
                }
                else if (_pfc.Families.Length == 1)
                {
                    _boldFamily = _pfc.Families[0];
                    _regularFamily = _pfc.Families[0];
                }
            }
            catch
            {
                _boldFamily = null;
                _regularFamily = null;
            }
        }


        // Font creation helpers
        private static Font CreateFont(float size, FontStyle style = FontStyle.Regular)
        {
            var family = (style == FontStyle.Bold) ? _boldFamily : _regularFamily;
            if (family != null)
                return new Font(family, size, style);
            return new Font("Segoe UI", size, style);
        }

        public static Font Bold(float size) => CreateFont(size, FontStyle.Bold);
        public static Font Regular(float size) => CreateFont(size, FontStyle.Regular);

        // ─── Type Scale ───
        // Display XL — Hero headings, splash screen
        public static Font DisplayXL => CreateFont(56f, FontStyle.Bold);
        // Display — Large section headings
        public static Font Display => CreateFont(44f, FontStyle.Bold);
        // H1 — Page titles
        public static Font H1 => CreateFont(32f, FontStyle.Bold);
        // H2 — Section headers
        public static Font H2 => CreateFont(24f, FontStyle.Bold);
        // H3 — Subsection headers
        public static Font H3 => CreateFont(20f, FontStyle.Bold);
        // Title — Card titles, dialog titles
        public static Font Title => CreateFont(16f, FontStyle.Bold);
        // Subtitle — Secondary headings
        public static Font Subtitle => CreateFont(14f, FontStyle.Regular);
        // Body — Primary body text
        public static Font Body => CreateFont(13f, FontStyle.Regular);
        // Body Small — Secondary body text
        public static Font BodySmall => CreateFont(12f, FontStyle.Regular);
        // Caption — Metadata, timestamps
        public static Font Caption => CreateFont(11f, FontStyle.Regular);
        // Label — Form labels, tags
        public static Font Label => CreateFont(11f, FontStyle.Bold);
        // Button — Button text
        public static Font Button => CreateFont(13f, FontStyle.Bold);
        // Button Small — Compact button text
        public static Font ButtonSmall => CreateFont(11.5f, FontStyle.Bold);
        // Status — Status badges, indicators
        public static Font Status => CreateFont(10f, FontStyle.Bold);
        // Numeric — Financial figures, data
        public static Font Numeric => new("Cascadia Code", 13f, FontStyle.Regular);
        // Monospace — Code, technical data
        public static Font Monospace => new("Cascadia Code", 11f, FontStyle.Regular);
    }


    // ═══════════════════════════════════════════════════════════════
    // SPACING — 8pt grid system with named tokens
    // ═══════════════════════════════════════════════════════════════

    public static class Spacing
    {
        public const int None = 0;
        public const int XXS = 2;   // Micro adjustments
        public const int XS = 4;    // Tight spacing
        public const int SM = 8;    // Default small gap
        public const int MD = 12;   // Medium gap
        public const int Base = 16; // Base unit
        public const int LG = 20;   // Large gap
        public const int XL = 24;   // Extra large
        public const int XXL = 32;  // Section spacing
        public const int XXXL = 40; // Major sections
        public const int Huge = 48; // Page margins
        public const int Giant = 64;// Hero spacing

        // Semantic spacing tokens
        public const int InputPaddingX = 14;
        public const int InputPaddingY = 10;
        public const int ButtonPaddingX = 20;
        public const int ButtonPaddingY = 10;
        public const int CardPadding = 20;
        public const int DialogPadding = 28;
        public const int SectionGap = 28;
        public const int PageMargin = 32;
        public const int FormGap = 16;
        public const int InlineGap = 8;
        public const int StackGap = 12;
    }


    // ═══════════════════════════════════════════════════════════════
    // RADIUS — Soft corners, consistent roundness
    // ═══════════════════════════════════════════════════════════════

    public static class Radius
    {
        public const int None = 0;
        public const int XS = 4;     // Badges, small chips
        public const int SM = 6;     // Inputs, buttons
        public const int MD = 8;     // Cards, dialogs
        public const int LG = 12;    // Large cards
        public const int XL = 16;    // Panels, modals
        public const int XXL = 20;   // Major containers
        public const int Full = 9999;// Pills, circles
    }

    // ═══════════════════════════════════════════════════════════════
    // ELEVATION — Shadow depths for layered surfaces
    // ═══════════════════════════════════════════════════════════════

    public static class Elevation
    {
        // Shadow parameters: (offsetX, offsetY, blur, spread, color)
        // Level 0 — Flat (no shadow)
        public static readonly Color Shadow0 = Color.FromArgb(0, 0, 0, 0);

        // Level 1 — Subtle lift (cards at rest)
        public static readonly Color Shadow1 = Color.FromArgb(8, 0, 0, 0);
        public const int Shadow1Blur = 8;
        public const int Shadow1OffsetY = 1;

        // Level 2 — Elevated (hovered cards, dropdowns)
        public static readonly Color Shadow2 = Color.FromArgb(12, 0, 0, 0);
        public const int Shadow2Blur = 16;
        public const int Shadow2OffsetY = 4;

        // Level 3 — Floating (modals, popovers)
        public static readonly Color Shadow3 = Color.FromArgb(20, 0, 0, 0);
        public const int Shadow3Blur = 32;
        public const int Shadow3OffsetY = 8;

        // Level 4 — Dramatic (splash elements)
        public static readonly Color Shadow4 = Color.FromArgb(32, 0, 0, 0);
        public const int Shadow4Blur = 48;
        public const int Shadow4OffsetY = 16;
    }


    // ═══════════════════════════════════════════════════════════════
    // MOTION — Animation timing, easing, and spring parameters
    // ═══════════════════════════════════════════════════════════════

    public static class Motion
    {
        // ─── Duration tokens (milliseconds) ───
        public const int Instant = 50;
        public const int Fast = 120;
        public const int Normal = 200;
        public const int Moderate = 300;
        public const int Slow = 450;
        public const int Deliberate = 600;
        public const int Dramatic = 900;
        public const int Epic = 1200;

        // ─── Frame timing ───
        public const int FrameInterval = 16; // ~60fps
        public const int SmoothFrameInterval = 8; // ~120fps for critical animations

        // ─── Spring physics parameters ───
        public const float SpringDamping = 0.72f;       // Smooth with slight overshoot
        public const float SpringStiffness = 0.08f;     // Medium spring
        public const float SpringMass = 1.0f;
        public const float SpringDampingSnappy = 0.85f; // Snappy, minimal overshoot
        public const float SpringStiffnessSnappy = 0.12f;
        public const float SpringDampingSoft = 0.6f;    // Soft, bouncy feel
        public const float SpringStiffnessSoft = 0.04f;

        // ─── Easing curves (cubic bezier approximations as step factors) ───
        // EaseOutExpo — Fast start, gentle landing (primary motion)
        public static float EaseOutExpo(float t) =>
            t >= 1f ? 1f : 1f - MathF.Pow(2f, -10f * t);

        // EaseOutCubic — Smooth deceleration
        public static float EaseOutCubic(float t) =>
            1f - MathF.Pow(1f - t, 3f);

        // EaseInOutCubic — Symmetrical ease
        public static float EaseInOutCubic(float t) =>
            t < 0.5f ? 4f * t * t * t : 1f - MathF.Pow(-2f * t + 2f, 3f) / 2f;

        // EaseOutBack — Slight overshoot (playful)
        public static float EaseOutBack(float t)
        {
            const float c1 = 1.70158f;
            const float c3 = c1 + 1f;
            return 1f + c3 * MathF.Pow(t - 1f, 3f) + c1 * MathF.Pow(t - 1f, 2f);
        }

        // EaseOutElastic — Bouncy spring
        public static float EaseOutElastic(float t)
        {
            if (t == 0f || t == 1f) return t;
            const float c4 = (2f * MathF.PI) / 3f;
            return MathF.Pow(2f, -10f * t) * MathF.Sin((t * 10f - 0.75f) * c4) + 1f;
        }

        // Linear
        public static float Linear(float t) => t;
    }


    // ═══════════════════════════════════════════════════════════════
    // SIZES — Component dimensions
    // ═══════════════════════════════════════════════════════════════

    public static class Sizes
    {
        // Button heights
        public const int ButtonHeightSM = 32;
        public const int ButtonHeightMD = 40;
        public const int ButtonHeightLG = 48;

        // Input heights
        public const int InputHeightSM = 32;
        public const int InputHeightMD = 40;
        public const int InputHeightLG = 48;

        // Icon sizes
        public const int IconXS = 12;
        public const int IconSM = 16;
        public const int IconMD = 20;
        public const int IconLG = 24;
        public const int IconXL = 32;

        // Badge / Chip
        public const int BadgeHeight = 22;
        public const int ChipHeight = 28;

        // Avatar
        public const int AvatarSM = 28;
        public const int AvatarMD = 36;
        public const int AvatarLG = 48;

        // Toast
        public const int ToastWidth = 380;
        public const int ToastMinHeight = 56;

        // Dialog
        public const int DialogMinWidth = 400;
        public const int DialogMaxWidth = 560;

        // Tray menu
        public const int TrayMenuWidth = 260;
        public const int TrayMenuItemHeight = 36;

        // Scrollbar
        public const int ScrollbarWidth = 6;
        public const int ScrollbarMinThumb = 32;

        // Focus ring
        public const int FocusRingWidth = 2;
        public const int FocusRingOffset = 2;
    }


    // ═══════════════════════════════════════════════════════════════
    // EFFECTS — Glass, blur, glow specifications
    // ═══════════════════════════════════════════════════════════════

    public static class Effects
    {
        // Glass morphism
        public const int GlassBlurRadius = 24;
        public const float GlassOpacity = 0.78f;
        public const float GlassBorderOpacity = 0.12f;

        // Glow
        public const int GlowRadiusSM = 8;
        public const int GlowRadiusMD = 16;
        public const int GlowRadiusLG = 32;
        public const int GlowRadiusXL = 64;

        // Noise texture
        public const float NoiseOpacity = 0.03f;

        // Particle effects
        public const int ParticleCount = 40;
        public const float ParticleMinOpacity = 0.02f;
        public const float ParticleMaxOpacity = 0.08f;
        public const float ParticleMinSpeed = 0.2f;
        public const float ParticleMaxSpeed = 0.8f;
        public const float ParticleMinSize = 10f;
        public const float ParticleMaxSize = 18f;
    }

    // ═══════════════════════════════════════════════════════════════
    // BREAKPOINTS — Responsive layout thresholds
    // ═══════════════════════════════════════════════════════════════

    public static class Breakpoints
    {
        public const int Compact = 800;
        public const int Medium = 1024;
        public const int Large = 1280;
        public const int XLarge = 1440;
        public const int Full = 1920;
    }
}
