/*
** i_sandbox_cocoa.mm
**
** macOS App Sandbox helpers (security-scoped bookmarks). See i_sandbox.h.
**
**---------------------------------------------------------------------------
**
*/

// Avoid collision between the engine's DObject "Class" and Objective-C.
#define Class ObjectClass
#include "m_argv.h"
#undef Class

#include "i_sandbox.h"

#include <stdlib.h>
#include <string.h>
#include <Cocoa/Cocoa.h>

// NSUserDefaults key holding an array of security-scoped bookmark blobs (NSData).
static NSString* const kWadFolderBookmarksKey = @"WadFolderBookmarks";

// URLs we are actively accessing, retained for the process lifetime so the
// security-scoped access is not released until exit.
static NSMutableArray* g_activeScopedURLs = nil;

static NSMutableArray* LoadBookmarkBlobs()
{
	NSArray* stored = [[NSUserDefaults standardUserDefaults] arrayForKey:kWadFolderBookmarksKey];
	return (nil != stored) ? [NSMutableArray arrayWithArray:stored] : [NSMutableArray array];
}

static void SaveBookmarkBlobs(NSArray* blobs)
{
	[[NSUserDefaults standardUserDefaults] setObject:blobs forKey:kWadFolderBookmarksKey];
	[[NSUserDefaults standardUserDefaults] synchronize];
}

static NSData* CreateBookmarkForURL(NSURL* url)
{
	NSError* error = nil;
	NSData* blob = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
				includingResourceValuesForKeys:nil
								 relativeToURL:nil
										 error:&error];
	if (nil == blob)
	{
		NSLog(@"Zandronum: could not create WAD-folder bookmark for %@: %@", url, error);
	}
	return blob;
}

static void StartAccessingURL(NSURL* url)
{
	if (nil == url)
	{
		return;
	}
	if (nil == g_activeScopedURLs)
	{
		g_activeScopedURLs = [[NSMutableArray alloc] init];
	}
	if ([url startAccessingSecurityScopedResource])
	{
		[g_activeScopedURLs addObject:url];
	}
	else
	{
		NSLog(@"Zandronum: could not start accessing WAD folder %@", url);
	}
}

void I_RestoreSandboxWadAccess(void)
{
	@autoreleasepool
	{
		NSMutableArray* blobs = LoadBookmarkBlobs();
		bool changed = false;

		for (NSUInteger i = 0; i < [blobs count]; ++i)
		{
			BOOL stale = NO;
			NSError* error = nil;
			NSURL* url = [NSURL URLByResolvingBookmarkData:[blobs objectAtIndex:i]
												  options:NSURLBookmarkResolutionWithSecurityScope
											relativeToURL:nil
									  bookmarkDataIsStale:&stale
													error:&error];
			if (nil == url)
			{
				NSLog(@"Zandronum: could not resolve WAD-folder bookmark: %@", error);
				continue;
			}

			StartAccessingURL(url);

			if (stale)
			{
				NSData* refreshed = CreateBookmarkForURL(url);
				if (nil != refreshed)
				{
					[blobs replaceObjectAtIndex:i withObject:refreshed];
					changed = true;
				}
			}
		}

		if (changed)
		{
			SaveBookmarkBlobs(blobs);
		}
	}
}

char* I_AddSandboxWadFolder(void)
{
	char* result = NULL;

	@autoreleasepool
	{
		// Ensure we can present a panel even if launched without a GUI session.
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		[NSApp activateIgnoringOtherApps:YES];

		NSOpenPanel* panel = [NSOpenPanel openPanel];
		[panel setCanChooseFiles:NO];
		[panel setCanChooseDirectories:YES];
		[panel setAllowsMultipleSelection:YES];
		[panel setResolvesAliases:YES];
		[panel setPrompt:@"Grant Access"];
		[panel setMessage:@"Select the folder(s) that contain your IWAD and PWAD files."];

		if (NSModalResponseOK != [panel runModal])
		{
			return NULL;
		}

		NSMutableArray* blobs = LoadBookmarkBlobs();
		NSMutableArray* grantedPaths = [NSMutableArray array];

		for (NSURL* url in [panel URLs])
		{
			NSData* blob = CreateBookmarkForURL(url);
			if (nil == blob)
			{
				continue;
			}
			[blobs addObject:blob];
			StartAccessingURL(url);
			[grantedPaths addObject:[url path]];
		}

		if ([grantedPaths count] > 0)
		{
			SaveBookmarkBlobs(blobs);
			const char* joined = [[grantedPaths componentsJoinedByString:@"\n"] UTF8String];
			if (NULL != joined)
			{
				result = strdup(joined);
			}
		}
	}

	return result;
}

bool I_IsNonInteractiveLaunch(void)
{
	if (Args == NULL)
	{
		return false;
	}
	return (Args->CheckParm("-iwad") != 0)
		|| (Args->CheckParm("+connect") != 0)
		|| (Args->CheckParm("-connect") != 0)
		|| (Args->CheckParm("-host") != 0)
		|| (Args->CheckParm("+host") != 0);
}

bool I_ShouldPromptForWadFolder(void)
{
	// Only sandboxed builds get a container id in the environment.
	if (getenv("APP_SANDBOX_CONTAINER_ID") == NULL)
	{
		return false;
	}

	@autoreleasepool
	{
		return [LoadBookmarkBlobs() count] == 0;
	}
}
