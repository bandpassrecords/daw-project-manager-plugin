# Repo secrets for macOS signing

Add these under **Settings → Secrets and variables → Actions**. They use the
**same names as the app's release job** (daw-project-manager's
`.github/REQUIRED_SECRETS.md`), so organisation-level secrets serve both repos.

## Signing the plug-in bundles (Gatekeeper "identified developer")

| Secret | Value |
|---|---|
| `APPLE_CERTIFICATE_BASE64` | `base64 -i YourCert.p12` of your **Developer ID Application** certificate (add the Developer ID Installer certificate to the same `.p12` to sign the `.pkg` too) |
| `APPLE_CERTIFICATE_PASSWORD` | password set when exporting the `.p12` |
| `APPLE_DEVELOPER_ID` | exact string, e.g. `Developer ID Application: Your Name (ABC123DEF4)` |
| `KEYCHAIN_PASSWORD` | any random string — only protects the throwaway CI keychain |

## Signing the installer package (new for the plugin)

| Secret | Value |
|---|---|
| `APPLE_INSTALLER_ID` | exact string, e.g. `Developer ID Installer: Your Name (ABC123DEF4)` |

An installer package can only be signed with a **Developer ID Installer**
certificate, which the app never needed (it ships a DMG and a ZIP). Without
it, the `.pkg` is built unsigned and Gatekeeper warns when it's opened; the
bundles inside, and the `.zip`, are still signed and notarized.

## Notarization

| Secret | Value |
|---|---|
| `APPLE_ID` | your Apple ID email |
| `APPLE_APP_SPECIFIC_PASSWORD` | appleid.apple.com → Sign-In and Security → App-Specific Passwords |
| `APPLE_TEAM_ID` | your 10-character Team ID |

## Behaviour when unset

Every signing and notarization step is skipped cleanly: the bundles are
ad-hoc signed (enough for Apple Silicon to load them) and the build succeeds.
That is how PR builds and forks run.

## Windows

The Windows installer is not code-signed, like the app's. SmartScreen may warn
on first download until the file builds reputation.
