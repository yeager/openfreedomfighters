# Intro player lifecycle boundary

Intro playback is not admitted to normal startup. The application has no verified project-owned player lifecycle that starts, updates, and completes the animated intro.

A future implementation must define construction, activation, update, completion, and teardown in project-owned terms and validate them with synthetic inputs. No original command layout, member count, callback order, or runtime behavior is published here.
