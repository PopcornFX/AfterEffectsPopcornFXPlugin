#include "ae_precompiled.h"
#include "AEGP_FileDialogMac.h"

#if defined(PK_OS_MACOSX)
#import <AppKit/NSOpenPanel.h>

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

#include <AE_GeneralPlugPanels.h>
#endif

#include <string>
#include <vector>

__AEGP_PK_BEGIN

#if defined(PK_OS_MACOSX)

CString		OpenFileDialogMac(const TArray<CString> &filters, const CString &defaultPathAndFile)
{
	TArray<CString> fileList;

	// Create a File Open Dialog class.
	NSOpenPanel* openDlg = [NSOpenPanel openPanel];
	[openDlg setLevel:CGShieldingWindowLevel()];

	// Set array of file types
	NSMutableArray * fileTypesArray = [NSMutableArray array];
	for (unsigned int i = 0; i < filters.Count(); i++)
	{
		NSString * filt =[NSString stringWithUTF8String:filters[i].Data()];
		[fileTypesArray addObject:filt];
	}

	// Enable options in the dialog.
	[openDlg setCanChooseFiles:YES];
	[openDlg setAllowedFileTypes:fileTypesArray];
	[openDlg setAllowsMultipleSelection:TRUE];

	if (!defaultPathAndFile.Empty())
	{
		[openDlg setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:defaultPathAndFile.Data() ] ] ];
	}

	// Display the dialog box. If the OK pressed,
	// process the files.
	if ( [openDlg runModal] == NSModalResponseOK )
	{
		// Gets list of all files selected
		NSArray *files = [openDlg URLs];
		// Loop through the files and process them.
		for (unsigned int i = 0; i < [files count]; i++ )
		{
			// Do something with the filename.
			fileList.PushBack(CString([[[files objectAtIndex:i] path] UTF8String]));
		}
	}
	if (fileList.Empty())
		return CString();
	return fileList[0];
}
#endif


__AEGP_PK_END
