# OTA Update Runbook

## 1) Release Channel Strategy

- **Stable channel (default):** regular releases without prerelease suffix (example: `0.2.0`).
- **Prerelease channel:** versions with suffix (example: `0.2.1-beta.1`, `0.2.1-rc.1`).
- App behavior is locked in code:
  - Stable builds do **not** opt into prerelease updates.
  - Prerelease builds keep receiving prerelease updates.
  - Optional override: `FINLOGS_UPDATE_CHANNEL=beta|alpha|prerelease|rc|canary`.

## 2) In-App Manual Update Flow

- Customer can manually trigger update from:
  - Sidebar → **Software Update**
  - Settings → **Check for Updates**
- Status events are shown in app and logged to `userData/updater.log`.
- **Restart & Install** appears only after update package is fully downloaded.

## 3) CI/CD Build + Publish

GitHub Actions workflow: `.github/workflows/release-ota.yml`

- Builds backend executable with PyInstaller.
- Builds Electron installer via `electron-builder`.
- Publishes release artifacts to GitHub Releases (`latest.yml`, installer, blockmap).

### Requirements

- Repository must allow workflow write access to releases.
- `package.json` `build.publish` is already configured for GitHub provider.

## 4) Staged Rollout Validation Checklist

Run this checklist for every new release:

1. **Internal smoke test (1-3 machines)**
   - Install current production build.
   - Trigger manual update check.
   - Verify download, restart/install, and data persistence.
2. **Limited customer rollout**
   - Share release only with pilot customers.
   - Monitor crash/error reports for 24-48 hours.
3. **Full rollout**
   - Announce to all customers after pilot passes.

## 5) Rollback and Recovery

If release has critical issue:

1. **Stop rollout immediately**
   - Mark bad GitHub release as draft or remove assets from latest release.
2. **Promote last known good version**
   - Re-publish previous stable release assets.
3. **Customer recovery path**
   - Install previous stable installer.
   - Keep/restore customer DB from backup if needed.
4. **Postmortem**
   - Capture `updater.log` and app logs.
   - Add regression check before next rollout.

## 6) Operational Commands

### Trigger local packaging

```bash
npm run dist
```

### Trigger backend build only

```bash
python -m PyInstaller backend.spec
```

---

Owner checklist before publish:

- [ ] Version updated in `package.json`
- [ ] Smoke test passed locally
- [ ] Release notes prepared
- [ ] CI workflow passed
- [ ] Pilot rollout completed
