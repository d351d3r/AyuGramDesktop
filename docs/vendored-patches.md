# Vendored patches

Third-party sources checked into this repository that were changed after import.
Re-vendoring an upstream release drops these changes, so re-apply every entry
below and update it with what upstream did in the meantime.

## sqlite_orm

- File: `Telegram/SourceFiles/ayu/libs/sqlite/sqlite_orm.h`
- Upstream: https://github.com/fnc12/sqlite_orm
- Imported from: `v1.9.1`, file `include/sqlite_orm/sqlite_orm.h`

The import is otherwise byte-identical to the release. To check for drift:

```bash
curl -sL https://raw.githubusercontent.com/fnc12/sqlite_orm/v1.9.1/include/sqlite_orm/sqlite_orm.h \
  | diff -u - Telegram/SourceFiles/ayu/libs/sqlite/sqlite_orm.h
```

### 1. Include the vendored sqlite3 amalgamation

`#include <sqlite3.h>` becomes `#include "sqlite3.h"` in 21 places, so the
header resolves against `sqlite3.h` sitting next to it instead of a system
SQLite. Predates the entries below.

### 2. `connection_holder::retain()` keeps its reference count balanced

`internal::connection_holder::retain()` raised `_retain_count` before calling
`sqlite3_open`, and threw on a failed open without lowering it again. The throw
leaves `connection_ref`'s constructor, so that object is never finished and its
destructor never calls `release()`. The count therefore stayed above zero for
the rest of the process: every later `retain()` saw a count above 1, skipped the
open, and handed out `db` as the failed open left it.

For AyuGram that meant one unusable database directory disabled storage for the
whole session. `AyuDatabase::initialize()` fails when `./tdata/` is missing, and
from then on even a directory the database *can* be created in kept failing with
`unable to open database file`, which put `AyuDatabase::moveCurrentDatabase()`
out of reach in exactly the case it exists for. Reads through the stale handle
also segfaulted.

The patch undoes the increment on the failing path, and closes the handle
`sqlite3_open` returns even when it fails so the next attempt does not overwrite
and leak it. `release()` additionally clears `db` after a successful close.

Upstream fixed the same defect on the `dev` branch by rewriting
`connection_holder` around a mutex and only counting a connection once its open
succeeded ("attention: only increase the reference count after successful open
in order to propagate a fully setup connection to other threads"). That rewrite
pulls in `vfs_name.h`, `db_open_mode.h`, `storage_options.h`, scope guards and
GSL, so it is not a candidate for a backport into the amalgamation. Reaching the
same invariant - the count never stays raised after a failed open - by undoing
the increment keeps the release-1.9.1 shape. **Drop this patch once a release
ships the `dev` branch rewrite.**

### Known residual, not patched

These share the same shape - a reference count raised before an operation that
can throw - but sit at the call sites rather than inside `retain()`, and
upstream only fixed them as part of the `dev` branch rewrite:

- `storage_base::begin_transaction_internal()` calls `connection->retain()`
  without a guard. If `on_open_internal()` or the `BEGIN` itself throws after a
  successful open, the count stays raised and the connection never closes. It
  keeps working - the handle is valid - but the database file stays open, which
  can block `moveCurrentDatabase()` on Windows.
- `storage_base::open_forever()` sets `isOpenedForever` before `retain()`. A
  failed open there leaves the flag set with a count of zero, and `~storage_base`
  then drives the count to -1. AyuGram never calls it.
- The in-memory branch of the `storage_base` constructors has the same unguarded
  `retain()`. AyuGram uses a file-backed database.
- `connection_ref::~connection_ref` calls `release()`, which throws when
  `sqlite3_close` fails. A destructor is `noexcept`, so that terminates. Present
  upstream as well.

### Coverage

`Telegram/SourceFiles/ayu/tests/ayu_database_tests.cpp`, test "initialize
survives a directory it cannot use". It runs `initialize()` and both reads and
writes against a directory the database cannot be created in, then asserts that
a healthy directory still works afterwards. Revert the patch and it fails,
taking every test that runs after it down with it.

```bash
sh Telegram/SourceFiles/ayu/tests/run_tests.sh
```
