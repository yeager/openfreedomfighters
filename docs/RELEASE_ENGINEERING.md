# Release engineering specification

This is the Phase 6 packaging, trust, migration, diagnostics, support, and
reproducibility contract. It defines a production pipeline, not current status.

Today GitHub builds data-free development archives for Linux x86-64, macOS, and
Windows x64 and performs a filename retail-extension audit plus Gitleaks. It does
not yet create native installers, sign Windows packages, sign/notarize macOS,
publish a Deck-qualified package, attest provenance/SBOMs, test migrations,
upload privacy-reviewed crash reports, or run clean-install campaign acceptance.

## Release identity and inputs

A production release comes only from a protected `vMAJOR.MINOR.PATCH` tag matching
the source version. Provenance records tag/commit/tree digest, clean state,
compiler/linker, target, options, dependencies and licenses/digests, runner image,
packaging tools, and signing/notarization result.

Jobs receive source and pinned public dependencies only. Retail data, private
analysis, compatibility traces, developer saves, screenshots, credentials, and
proprietary SDK material are forbidden inputs. User-owned retail data remains a
runtime prerequisite and is never packaged.

Artifacts include a file manifest, dependency evidence, SPDX or CycloneDX SBOM,
checksums, and signed provenance tied to workflow identity. Credentials are
isolated in protected release environments, available only to signing steps,
never logged, and unavailable to pull-request workflows.

## Platform packages

### Windows

Ship native x64 first. Include only the application, permitted runtimes,
licenses, support documents, and manifest. Production executables and installer
are Authenticode-signed and timestamped; verify every binary after packaging.
Test install, upgrade, repair if offered, uninstall, and standard-user execution.
Mutable state lives outside the install directory.

### macOS

Ship a structured `.app` in a signed DMG or documented native container. Declare
arm64/x86-64 coverage honestly. Sign nested code inside-out with hardened runtime
and minimum justified entitlements, submit the final artifact for notarization,
staple it, then verify signature and Gatekeeper acceptance after download.
Rosetta does not qualify a native architecture.

### Linux and Steam Deck

Ship a relocatable native package with declared minimum runtime and audited
dependency strategy, desktop metadata, icons, licenses, and launch instructions.
Use XDG paths for configuration, saves, caches, and opt-in diagnostics. A common
x86-64 payload becomes Deck-supported only after native SteamOS hardware tests of
controller, display, suspend/resume, storage, performance, and campaign. Proton
or Wine is not native qualification.

Every package manifest lists paths, modes, digests, origins, licenses, and file
roles. Unexpected files fail packaging.

## Retail-data discovery

First launch checks explicit configuration, supported store locations, then a
user-selected directory in documented order. Detection is read-only. The verifier
reports aggregate actionable failures without copying, patching, or extracting
retail assets into the application package.

With no valid installation, the UI explains that purchased PC data is required
and offers a native directory selector; it never suggests unlicensed downloads.
Saved paths contain no credential and remain user-editable.

## Configuration, save, and cache migration

Configuration, profiles, saves, and caches have separate version/ownership
boundaries. Each release names the supported upgrade matrix and behavior for
newer, unknown, corrupt, and interrupted state.

Migrations are ordered, idempotent, bounded, and transactional: validate source,
write and flush a new generation beside the destination, atomically replace where
supported, and retain recovery until validation succeeds. They never alter retail
files. Downgrade is tested or explicitly rejected. Newer schemas are never
silently interpreted. Disposable caches rebuild when schema, engine, ABI, or
retail-data fingerprint changes.

Tests cover fresh state, each upgrade path, interruption at every commit boundary,
truncation/corruption, low disk, read-only destination, concurrent launch, and
recovery.

## Crash privacy

Local crash capture requires no upload. Upload is off by default and requires
informed, revocable opt-in stating fields, purpose, destination, retention, and
inspection/deletion controls. Declining cannot reduce compatibility.

The upload allowlist is application version/commit, OS/architecture,
module-relative project stack addresses, exception/signal, graphics backend and
non-unique capability flags, project-authored scene ID, and recent project
diagnostic codes. Before persistence and upload, scrub absolute paths, usernames,
environment/command line, retail names/content, memory dumps, screenshots,
dialogue, saves, device/store/account identifiers, IP addresses, and secrets.
Default upload never includes full memory dumps.

Reports have bounded documented schemas, TLS, authenticated endpoints, rate/size
limits, retention deletion, and access control. Users preview exact payloads.
Symbols remain private by release identity. Offline export receives the same
scrubbing. Synthetic canaries prove redaction on every platform, and privacy
review blocks release.

## Final-package audit

Audit the final signed downloadable bytes, not only staging. Bounded recursive
unpacking verifies the manifest and rejects extra/missing files, escaping links,
unsafe permissions, absolute paths, private/debug artifacts, credentials,
prohibited retail extensions, known retail fingerprints, and oversized members.
Filename scanning alone is insufficient.

The gate also runs full-history and release-tree Gitleaks, dependency vulnerability
and license policy, SBOM comparison, malware scanning where available,
signature/notarization verification, and runner-path/timestamp checks. Logs redact
workspace/user paths and never echo secrets or retail identifiers.

## Clean-install qualification

Download each candidate by checksum from its real distribution surface and test
on clean supported OS images. Attach user-owned data only in private qualification
after package auditing. Record OS, architecture, hardware/API/driver, digest,
install mode, data fingerprint, input device, and result.

Test install, missing-data launch, data selection/verification, settings, save and
resume, F10 apply/rollback, controller/audio, suspend/resume where applicable,
upgrade/migration/recovery, uninstall, and fresh-start campaign completion in
Original. Performance-test Modern and prove equal authoritative hashes. Deck runs
on hardware. Hosted compilation or startup smoke is not campaign evidence.

Failures block release unless the configuration is removed from the published
support matrix. Exceptions need a public time-bounded rationale and cannot weaken
security or retail-data policy.

## Support and rollback

Publish installation/data-location steps, supported OS/architecture/GPU/data
matrix, controls, graphics fallback, state locations, privacy-safe log collection,
migration/backup, known issues, troubleshooting, licenses, checksums/signatures,
and security contact.

Safe mode resets presentation without deleting saves and disables optional
Modern+ paths. Cache rebuild, settings reset, and save deletion remain distinct;
destructive support steps require exact target and confirmation.

Keep the prior production package and migration compatibility during rollout.
Never replace published bytes silently: withdraw distribution if necessary and
publish a new signed corrective release/advisory. A `SECURITY.md` reporting policy
is required before production readiness.

## Reproducibility

Pin inputs by digest; normalize archive order, ownership, modes, locale, timezone,
paths, and timestamps. A second isolated builder reconstructs unsigned payloads
and compares digests. Signing/notarization are nondeterministic envelopes, so
compare documented pre-signing content and separately prove that the envelope
contains exactly it. Any difference produces a file/section diff and blocks the
release; provenance names both builders and comparison.

## Production checklist

1. Campaign, localization, presentation, save, and platform gates pass.
2. Version, tag, schemas, changelog, support and upgrade matrices agree.
3. Tests, sanitizers, dependency/license policy, and Gitleaks pass.
4. Two builders agree on unsigned payloads; SBOM, manifest, checksums, and
   provenance are complete.
5. Final packages pass recursive retail, secret, permission, and manifest audit.
6. Windows signing/timestamp, macOS signing/notarization/stapling, and Linux checks
   pass on downloaded bytes.
7. Clean install, upgrades, interruption recovery, uninstall, and native hardware
   qualification pass on every advertised target.
8. Crash upload remains opt-in and passes exact-payload privacy review.
9. Support, security, rollback, licenses, checksums, signatures, and known issues
   are published.

No Phase 6 roadmap item is complete today. Development archives are useful CI
artifacts, not signed, notarized, privacy-reviewed, clean-install-qualified
production releases.
