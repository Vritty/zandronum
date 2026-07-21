/*
** i_sandbox.h
**
** macOS App Sandbox helpers (security-scoped bookmarks) that let the sandboxed
** App Store build read the user's WAD folders, and thus absolute -iwad/-file
** paths handed to it on the command line (e.g. by Doomseeker).
**
** This is macOS-only; the implementation lives in i_sandbox_cocoa.mm.
**
**---------------------------------------------------------------------------
**
*/

#ifndef __I_SANDBOX_H__
#define __I_SANDBOX_H__

#ifdef __cplusplus
extern "C" {
#endif

// Resolves every persisted WAD-folder bookmark and begins accessing it for the
// lifetime of the process. Safe to call once at startup; a no-op when there are
// no stored bookmarks. Never prompts.
void I_RestoreSandboxWadAccess(void);

// Presents a native folder picker so the user can grant access to one or more
// WAD folders, persisting a security-scoped bookmark for each. Returns a
// malloc'd, newline-separated list of the granted folder paths (caller frees),
// or NULL if cancelled. Only call from an interactive launch.
char* I_AddSandboxWadFolder(void);

// True when the process was started non-interactively (Doomseeker etc. always
// pass -iwad, and usually a server to connect to). Such launches have no usable
// foreground GUI, so blocking startup modals must be skipped to avoid a hang.
bool I_IsNonInteractiveLaunch(void);

// True only when the process is running inside the App Sandbox and the user has
// not yet granted access to any WAD folder. Used to show a one-time console tip
// about the "addwadfolder" command. Always false for non-sandboxed builds.
bool I_ShouldPromptForWadFolder(void);

#ifdef __cplusplus
}
#endif

#endif // __I_SANDBOX_H__
