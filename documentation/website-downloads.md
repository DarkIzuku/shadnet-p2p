<!--
SPDX-FileCopyrightText: Copyright 2026 shadNet Project
SPDX-License-Identifier: GPL-2.0-or-later
-->

# The Hunter's Requiem downloads

The public catalog is available at `/downloads`. It lists active files only and serves each file as
`application/octet-stream` with `Content-Disposition: attachment`. `/api/downloads` and
`/api/downloads/<id>` are read-only public metadata endpoints.

Authenticated server administrators use `/admin/downloads`. Every write endpoint requires a valid
web session, the `account.admin` flag, same-origin validation, and the session CSRF token. Visitors
and normal accounts cannot upload, edit, replace, activate, deactivate, or delete files.

Files are stored in `data/downloads/`, outside the web asset directory. Internal filenames are
random UUIDs without extensions, are never supplied by the browser, and are never returned by an
API. The original filename is rejected if it contains path syntax and is sanitized before being
stored as display metadata. SHA-256 and file size are calculated by the server from the received
bytes; browser content types are ignored.

`BloodborneWebsiteDownloadMaxFileSizeMiB` controls the upload and replacement limit. The default is
512 MiB and accepted values are clamped to 1-2048 MiB.

For a complete backup, stop shadNet and copy the SQLite database (plus any `-wal`/`-shm` files if
present) together with the full `data/downloads/` directory. Never place executable web content in
the `web/` assets directory as a substitute for this catalog.
