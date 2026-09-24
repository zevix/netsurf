/*
 * Copyright 2026 The netsurf_upy contributors
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * A script-engine plugin from the future, for the refusal path.
 *
 * Added 2026-09-25 (netsurf_upy, GPLv2 section 2(a) change notice).
 *
 * `engine.c::engine_load()` refuses a plugin whose ABI token is not
 * this browser's and carries on with the engine it was built with.
 * That branch is only worth having if something exercises it, and the
 * only honest way to exercise it is with an object that really does
 * load, really does export `ns_script_engine_v1_get` and really does
 * answer with the wrong number.  This is that object.
 *
 * It deliberately includes **none** of NetSurf's headers, not even
 * `javascript/engine.h`: a plugin built somewhere else, some other
 * year, against some other revision of the vtable is exactly the case
 * being tested, and a fixture that shared the browser's declarations
 * would be testing something narrower.  The two fields the loader may
 * read before it decides -- a 32-bit ABI token and a name pointer --
 * are declared here from scratch, which is all the layout an
 * ABI-mismatched plugin can be relied on to get right.
 *
 * It is built only when `NETSURF_DUKTAPE_PLUGIN := YES`, and it is not
 * installed by the browser's own install rule.
 *
 * > **Corrected 2026-09-25 (G4 audit, item 1).**  The sentence above is
 * > true of NetSurf's `make install` and misleading about what ships,
 * > which is the only question a later phase will be asking.  This
 * > object *is* installed: `RD/nix/flake.nix`'s `postInstall` takes it
 * > out of the build directory by hand, because `make install` knows
 * > nothing about `POSTEXES`.  What is true -- and is now an
 * > arrangement rather than an accident (G4 audit, item 2) -- is that
 * > it is installed **beside** the engines and not among them, to
 * > `$out/lib/netsurf-test/`, and that it reaches exactly one image:
 * > the `<board>-netsurf-abi-mismatch-probe` the ABI-mismatch arm
 * > boots, which names this file explicitly.  `netsurfProbeFor` takes
 * > the objects it installs as an argument and globs nothing, so the
 * > ordinary probe image does not carry this fixture and no future
 * > fixture can ship by being written.
 */

#include <stdint.h>

/** Not NS_SCRIPT_ENGINE_ABI_V1, on purpose, and never will be. */
#define NS_SCRIPT_ENGINE_ABI_FROM_THE_FUTURE ((uint32_t) 0x6e734639)	/* "nsF9" */

/**
 * As much of the vtable as a mismatched plugin can promise: the token
 * the loader checks, and the name it may put in its log line.  What
 * follows the name is, by hypothesis, a table this browser does not
 * know the shape of -- so there is nothing after it and the loader must
 * not read past `name`.
 */
struct ns_script_engine_head {
	uint32_t abi;
	const char *name;
};

static const struct ns_script_engine_head from_the_future = {
	.abi = NS_SCRIPT_ENGINE_ABI_FROM_THE_FUTURE,
	.name = "abi-mismatch",
};

const struct ns_script_engine_head *ns_script_engine_v1_get(const void *core);

const struct ns_script_engine_head *ns_script_engine_v1_get(const void *core)
{
	(void) core;

	return &from_the_future;
}
