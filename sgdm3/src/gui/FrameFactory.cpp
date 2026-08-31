// FrameFactory.cpp
//////////////////////////////////////////////////////////////////

#include <ConfigPch.h>
#include <ConfigDcl.h>
#include <util.h>
#include <fim.h>
#include <poi.h>
#include <fd.h>
#include <fs.h>
#include <xt.h>
#include <gui.h>

//////////////////////////////////////////////////////////////////

FrameFactory::FrameFactory(void)
{
}

FrameFactory::~FrameFactory(void)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

//	wxLogTrace(TRACE_GUI_DUMP,
//			   _T("FrameFactor::~FrameFactory: [nr %ld]"),
//			   (long)m_map.size());

	// WARNING: don't delete gui_frames here -- they should already be gone -- see gui_app::OnExit().
	wxASSERT_MSG( (m_map.empty()),
				  wxString::Format(_T("FrameFactory: map not empty. [nr %ld]"),
								   (long)m_map.size()));
}

//////////////////////////////////////////////////////////////////

gui_frame * FrameFactory::newFrame(ToolBarType tbt, const wxString & caption)
{
	gui_frame * pFrame = new gui_frame(caption,tbt);

	{
		// this lock will be around until 'lock' goes out of scope
		wxCriticalSectionLocker lock(m_cs_map);
		m_map.insert(TMapValue(pFrame,pFrame));
	}

	return pFrame;
}

gui_frame * FrameFactory::_createFirstFrame(const cl_args * pArgs)
{
	// create the raw,unpopulated first frame window using hints
	// from the command line arguments.  that is, we select the
	// type (what kind of menu/toolbar) and set the caption on
	// the frame.

	return newFrame(gui_frame::suggestInitialType(pArgs),
					(pArgs) ? pArgs->getFrameCaption() : VER_APP_TITLE);
}

gui_frame * FrameFactory::openFirstFrame(const cl_args * pArgs,
										 const xt_tool ** ppxt, wxString & rStrXtExe, wxString & rStrXtArgs)
{
	wxASSERT_MSG( (pArgs), _T("Coding Error -- cl_args expected.") );
	if (!pArgs || (pArgs->nrParams == 0))
	{
		// if they didn't give us any files/folders, we always have exit status 0.
		wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__OK);

		// create the empty first frame to parent any dialogs.
		gui_frame * pFrameFirst = _createFirstFrame(pArgs);

		// no command-line args: behave as if the user immediately picked
		// File > Open Folder Diff...  raise the Open Folders dialog and, if
		// they pick a pair, recycle this empty frame into a folder diff.
		// if they cancel, they just keep the empty window.
		wxString s0, s1;
		if (_raiseOpenFoldersDialog(pFrameFirst,NULL,s0,s1))
		{
			gui_frame * pFrameLoaded = openFolderFrame(s0,s1,NULL);
			if (pFrameLoaded)
				return pFrameLoaded;
		}

		return pFrameFirst;
	}
	
	// if only one pathname given on command line -- possibly because the user
	// drug a file/folder onto our exe's desktop icon.  launch modified
	// version of dlg_open with the one pathname filled in and the other
	// blank and let them complete the set.
	//
	// if multiple files/directories given on command line, go ahead and open them.
	// but if a file/folder is duplicated, we ask for confirmation first.
	// if they don't want duplicates, raise dlg_open with all of the pathnames
	// prepopulated with the command line args.
	//
#ifdef FEATURE_SHEX
	// When we get called from Windows Explorer (the Shell) and we have more than
	// one pathname, we get them in a random order.  so, force the open interlude
	// dialog up so that they can swap the files/folders if necessary before we
	// open them.
	//
	// Currently, the shell extension module will only send us 1 or 2 pathnames
	// (diffs only; no 3-ways).
	//
	// TODO this is kind of a screw case by extension, but when we raise the open
	// TODO interlude dialog we should keep track of whether the user does a swap
	// TODO so that we can also swap the titles if given.  but we can raise the
	// TODO open dialog many times (see the while() loop in _raise...()).  this
	// TODO will probably never actually be needed because when we get the /shex
	// TODO flag from Explorer, we won't get titles.
#endif
	// 
	// if we couldn't open one of the files, we throw up
	// an error dialog and continue -- that is, we let this fail and create
	// an empty window as if nothing was given on the command line.
	//
	// if the delay-load code gets an error later trying to populate the frame
	// that we have purposed, it will revert it back to an empty window later.

	wxString s0, s1, s2;
	gui_frame * pFrameFirst = NULL;		// don't create empty frame unless we need to ask something.
	gui_frame * pFrameLoaded = NULL;

	if (pArgs->bFolders)
	{
		// we have at least one folder.

		// if we have 2 and they are unique -or- they're ok with duplicates (and we were
		// not forced to use the open-interlude), open them.
		// 
		// otherwise (we have 1 folder or duplicates (or if we were forced)), ask them
		// for pathnames.  if they say ok, open them.

		// since we don't support external tools for folder, we can go ahead
		// an create an empty frame to parent any dialogs that we ask before
		// we actually populate the frame.

		pFrameFirst = _createFirstFrame(pArgs);

		if (   (pArgs->nrParams == 2)
#ifdef FEATURE_SHEX
			&& (!pArgs->bShEx)			// if bShEx, force dlg to appear
#endif
			&& (_checkForUniqueFolders(pFrameFirst,pArgs->pathname[0],pArgs->pathname[1])))
		{
			pFrameLoaded = openFolderFrame(pArgs->pathname[0],pArgs->pathname[1],pArgs);
			// verify frame was created and that it recycled the empty frame created to
			// parent the dialogs.
			wxASSERT_MSG( (pFrameLoaded && (pFrameLoaded==pFrameFirst)), _T("Coding error!"));

			// we currently don't define an exit value (other than 0) for 2-way windows.
			wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__OK);

			return pFrameLoaded;
		}
		else if (_raiseOpenFoldersDialog(pFrameFirst,pArgs,s0,s1))
		{
			pFrameLoaded = openFolderFrame(s0,s1,pArgs);
			// verify frame was created and that it recycled the empty frame created to
			// parent the dialogs.
			wxASSERT_MSG( (pFrameLoaded && (pFrameLoaded==pFrameFirst)), _T("Coding error!"));

			// we currently don't define an exit value (other than 0) for 2-way windows.
			wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__OK);

			return pFrameLoaded;
		}

		// they canceled the open-interlude, return the empty window we created to
		// parent the dialogs.

		wxASSERT_MSG( (pFrameFirst), _T("Coding error!") );

//		WARNING: don't call revertToEmptyFrame() this causes problems (at least on windows)
//		WARNING: where after a subsequent binding to a particular type of window, resize
//		WARNING: events don't work properly in the window (the frame does not resize the
//		WARNING: the child view).  wxWidgets automatically resizes the child when there is
//		WARNING: exactly one (not counting the toolbar or status bar), but re-binding seems
//		WARNING: to confuse it -- as if it doesn't completely get rid of the original one.
//		pFrameFirst->revertToEmptyFrame();

		// we currently don't define an exit value (other than 0) for 2-way windows.
		wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__OK);

		return pFrameFirst;
	}

	const xt_tool * pxt = NULL;

	// we have at least one file.

	if (pArgs->nrParams == 3)		// we are trying to open a merge window
	{
		// if we have 3 unique pathnames -or- they are ok with duplicates, open them.
		//
		// otherwise (we have duplicates and they don't like that), ask them for
		// pathnames.  if they say ok, open them.

		if (_checkForUniqueFiles(&pFrameFirst,pArgs,pArgs->pathname[0],pArgs->pathname[1],pArgs->pathname[2]))
		{
			pFrameLoaded = openFileMergeFrame(pArgs->pathname[0],pArgs->pathname[1],pArgs->pathname[2],
											  pArgs,
											  &pxt,rStrXtExe,rStrXtArgs);
			if (pxt)
				goto HandleExternalTool;

			// verify frame was created and that it recycled the empty frame created to
			// parent the dialogs, if we needed one.
			wxASSERT_MSG( (pFrameLoaded && ((pFrameLoaded==pFrameFirst) || (pFrameFirst==NULL))),
						  _T("Coding error!"));

			goto RegisterInitialMergeFrame;
		}
		else if (_raiseOpenFileMergeDialog(&pFrameFirst,pArgs,s0,s1,s2))
		{
			pFrameLoaded = openFileMergeFrame(s0,s1,s2,
											  pArgs,
											  &pxt,rStrXtExe,rStrXtArgs);
			if (pxt)
				goto HandleExternalTool;

			// verify frame was created and that it recycled the empty frame created to
			// parent the dialogs.  (we know we needed one.)
			wxASSERT_MSG( (pFrameLoaded && (pFrameLoaded==pFrameFirst)),
						  _T("Coding error!"));

			goto RegisterInitialMergeFrame;
		}
		else
		{
			// they canceled the open-interlude, return the empty window we created to
			// parent the dialog.

			wxASSERT_MSG( (pFrameFirst), _T("Coding error!") );
//			WARNING: don't call revertToEmptyFrame() this causes problems (at least on windows)
//			pFrameFirst->revertToEmptyFrame();

			// when they cancel the open we give them an empty window.  but if a script
			// launched us and is waiting for a merge result, we won't have one to give
			// them.  force an error value for our exit status so that the script doesn't
			// try to use the /result that we didn't create....
			wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__FILE_ERROR);

			return pFrameFirst;
		}
	}
	else
	{
		// we have 1 or 2 files.

		// if we have 2 unique pathnames (or they are ok with duplicates) (and we
		// we not forced to raise the open-interlude, open them.
		//
		// if we have 1 file or duplicates or were forced to raise the open-interlude
		// raise it and ask for pathnames.  if they say ok, open them.

		if (   (pArgs->nrParams == 2)
#ifdef FEATURE_SHEX
			&& (!pArgs->bShEx)			// if bShEx, force dlg to appear
#endif
			&& (_checkForUniqueFiles(&pFrameFirst,pArgs,pArgs->pathname[0],pArgs->pathname[1])))
		{
			pFrameLoaded = openFileDiffFrame(pArgs->pathname[0],pArgs->pathname[1],
											 pArgs,
											 &pxt,rStrXtExe,rStrXtArgs);
			if (pxt)
				goto HandleExternalTool;

			// verify frame was created and that it recycled the empty frame created to
			// parent the dialogs, if we needed one.
			wxASSERT_MSG( (pFrameLoaded && ((pFrameLoaded==pFrameFirst) || (pFrameFirst==NULL))),
						  _T("Coding error!"));

			// we currently don't define an exit value (other than 0) for 2-way windows.
			wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__OK);

			return pFrameLoaded;
		}
		else if (_raiseOpenFileDiffDialog(&pFrameFirst,pArgs,s0,s1))
		{
			pFrameLoaded = openFileDiffFrame(s0,s1,
											 pArgs,
											 &pxt,rStrXtExe,rStrXtArgs);
			if (pxt)
				goto HandleExternalTool;

			// verify frame was created and that it recycled the empty frame created to
			// parent the dialogs.  (we know we needed one.)
			wxASSERT_MSG( (pFrameLoaded && (pFrameLoaded==pFrameFirst)),
						  _T("Coding error!"));

			// we currently don't define an exit value (other than 0) for 2-way windows.
			wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__OK);

			return pFrameLoaded;
		}

		// they canceled the open-interlude, return the empty window that we created.

		wxASSERT_MSG( (pFrameFirst), _T("Coding error!") );
//		WARNING: don't call revertToEmptyFrame() this causes problems (at least on windows)
//		pFrameFirst->revertToEmptyFrame();

		// we currently don't define an exit value (other than 0) for 2-way windows.
		wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__OK);

		return pFrameFirst;
	}

HandleExternalTool:
	// Caller should spawn an external tool using (rStrXtExt,rStrXtArgs).
	// Since this is only called by main(), it should just exec the
	// external tool and exit (and not have any DiffMerge windows
	// open).  if we created an empty window (in pFrameFirst) (to
	// properly parent any dialogs that we raised), we should destroy it.
	// ***BUT*** this can cause problems because we don't know if the
	// main event loop is running and Close/Destroy does a deferred-delete
	// (deletes some stuff and queues the rest for the next idle time).
	// let's return the frame and let the caller deal with it.

	wxASSERT_MSG( (pFrameLoaded==NULL), _T("Coding Error!"));

	*ppxt = pxt;
	return pFrameFirst;


RegisterInitialMergeFrame:
	// since we actually opened the set of files given on the command line,
	// we want our exit status to reflect the result of the user's merge
	// *IF* we were given a /result file.  if we are a normal merge with a
	// /result file, we do not register to set the exit status.
	//
	// we also want to remember the original result file that our invoker
	// is expecting us to write to (so we can distinguish save from save-as).

	fs_fs * pFsFs = pFrameLoaded->m_pDoc->getFsFs();
	poi_item * pPoiOriginalResult = pFsFs->getResultPOI();
	if (pPoiOriginalResult)
		wxGetApp().setExitStatusWaitForMergeResult(pFrameLoaded,pPoiOriginalResult);
	else
		wxGetApp().setExitStatusStatic(MY_EXIT_STATUS__OK);

	return pFrameLoaded;
}

//////////////////////////////////////////////////////////////////

gui_frame * FrameFactory::openFolderFrame(const wxString & s0, const wxString & s1, const cl_args * pArgs)
{
	// open folderdiff frame window (as necessary).
	// 
	// if folder pair is already open in an existing frame somewhere, raise it.
	// otherwise, if we have an empty frame (probably from startup), convert it to a folderdiff.
	// otherwise, create a new frame and open a folderdiff.
	//
	// we use FrameFactory's raiseFrame() (rather than wxFrame::Raise()) to deal with platform issues.
	//
	//////////////////////////////////////////////////////////////////
	// NOTE: our caller should have checked for (s0 == s1) and run thru the
	// NOTE: dont-show-again-msgbox for confirmation on VIEW_FOLDER_DONT_SHOW_SAME_FOLDER_MSGBOX.
	// NOTE: we don't do it here because we don't have a parent frame window yet.
	//////////////////////////////////////////////////////////////////

	fd_fd * pFdFd = gpFdFdTable->find(s0,s1);
	if (pFdFd)
	{
		gui_frame * pFrame = findFolderFrame(pFdFd);
		wxASSERT_MSG( (pFrame), _T("FrameFactory::openFolderFrame: fd_fd exists but frame does not.") );
		raiseFrame(pFrame);

		return pFrame;
	}

	gui_frame * pFrame = findEmptyFrame();
	if (!pFrame)
		pFrame = newFrame(TBT_FOLDER, ((pArgs) ? pArgs->getFrameCaption() : VER_APP_TITLE));
	else
		raiseFrame(pFrame);
	pFrame->_loadFolders(s0,s1,pArgs);

	return pFrame;
}

//////////////////////////////////////////////////////////////////

gui_frame * FrameFactory::openFileDiffFrame(const wxString & s0, const wxString & s1,
											const cl_args * pArgs,
											const xt_tool ** ppxt, wxString & rStrXtExe, wxString & rStrXtArgs)
{
	// open file-diff frame window (as necessary).  [this is not to create an additional view
	// frame on an existing window.]
	// 
	// if file pair is already open in an existing frame somewhere, raise it.
	// we'll take the first gui_frame we find that contains them.
	// 
	// if the set of files should be opened by an external tool, return
	// the tool handle and let the caller deal with spawning it.
	//
	// otherwise, if we have an empty frame (probably from startup), convert it to a file-diff.
	// otherwise, create a new frame and open a file-diff.
	//
	// we use FrameFactory's raiseFrame() (rather than wxFrame::Raise()) to deal with platform issues.

	fs_fs * pFsFs = gpFsFsTable->find(s0,s1);
	if (pFsFs)
	{
		gui_frame * pFrame = findFileDiffFrame(pFsFs);
		wxASSERT_MSG( (pFrame), _T("FrameFactory::openFileDiffFrame: fs_fs exists but frame does not.") );
		raiseFrame(pFrame);

		return pFrame;
	}

	// lookup an external tool configured for this set of files.
	// if we found one (and the user wants to use it), return NULL
	// and set *ppxt,rStrXtExe,rStrXtArgs.

	if (FrameFactory::lookupExternalTool(2,s0,s1,wxString(),pArgs,ppxt,rStrXtExe,rStrXtArgs))
		return NULL;

	// open one of our windows normally.

	gui_frame * pFrame = findEmptyFrame();
	if (!pFrame)
		pFrame = newFrame(TBT_DIFF,((pArgs) ? pArgs->getFrameCaption() : VER_APP_TITLE));
	else
		raiseFrame(pFrame);
	pFrame->_loadFileDiff(s0,s1,pArgs);

	return pFrame;
}

//////////////////////////////////////////////////////////////////

gui_frame * FrameFactory::openFileMergeFrame(const wxString & s0, const wxString & s1, const wxString & s2,
											 const cl_args * pArgs,
											 const xt_tool ** ppxt, wxString & rStrXtExe, wxString & rStrXtArgs)
											 
{
	// open file-merge frame window (as necessary).  [this is not to create an additional view
	// frame on an existing window.]
	// 
	// if file set is already open in an existing frame somewhere, raise it.  
	// we'll take the first gui_frame we find that contains them.
	//
	// if the set of files should be opened by an external tool, return
	// the tool handle and let the caller deal with spawning it.
	//
	// otherwise, if we have an empty frame (probably from startup), convert it to a file-merge.
	// otherwise, create a new frame and open a file-merge.
	//
	// we use FrameFactory's raiseFrame() (rather than wxFrame::Raise()) to deal with platform issues.

	fs_fs * pFsFs = gpFsFsTable->find(s0,s1,s2);
	if (pFsFs)
	{
		gui_frame * pFrame = findFileMergeFrame(pFsFs);
		wxASSERT_MSG( (pFrame), _T("FrameFactory::openFileMergeFrame: fs_fs exists but frame does not.") );
		raiseFrame(pFrame);

		return pFrame;
	}

	// lookup an external tool configured for this set of files.
	// if we found one (and the user wants to use it), return NULL
	// and set *ppxt,rStrXtExe,rStrXtArgs.

	if (FrameFactory::lookupExternalTool(3,s0,s1,s2,pArgs,ppxt,rStrXtExe,rStrXtArgs))
		return NULL;

	// otherwise, open one of our windows normally.
	
	gui_frame * pFrame = findEmptyFrame();
	if (!pFrame)
		pFrame = newFrame(TBT_MERGE,((pArgs) ? pArgs->getFrameCaption() : VER_APP_TITLE));
	else
		raiseFrame(pFrame);
	pFrame->_loadFileMerge(s0,s1,s2,pArgs);

	return pFrame;
}

//////////////////////////////////////////////////////////////////

void FrameFactory::openFoldersFromDialogs(gui_frame * pDialogParent)
{
	// using the given window as a dialog parent, ask for a set of
	// folder pathnames and then open them in a new or recycled window.

	wxString s0, s1;

	if (_raiseOpenFoldersDialog(pDialogParent,NULL,s0,s1))
		openFolderFrame(s0,s1,NULL);
}

bool FrameFactory::_checkForUniqueFolders(gui_frame * pDialogParent,
										  const wxString & s0,		// input only
										  const wxString & s1)		// input only
{
	// TODO put these pathnames into wxFileName and normalize them and
	// TODO then compare the normalized result -- so that "." and "./"
	// TODO compare equal, for example.

	if (s0.Cmp(s1) != 0)	// different pathnames, accept them.
		return true;
		
	// same pathname for both, ask for confirmation.

	int result = util_dont_show_again_msgbox::msgbox(pDialogParent,
													 _("Warning"),
													 wxGetTranslation(L"The folder pathnames refer to the same directory.\n"
																	  L"The Folder Diff Window will not report any differences.\n"
																	  L"\n"
																	  L"Are you sure you want to open this window?"),
													 GlobalProps::GPL_VIEW_FOLDER_DONT_SHOW_SAME_FOLDER_MSGBOX,
													 wxYES|wxNO);
	if (result == wxID_OK)	// they really want to do open the same path in both sides
		return true;

	return false;
}
	
bool FrameFactory::_raiseOpenFoldersDialog(gui_frame * pDialogParent,
										   const cl_args * pArgs,
										   wxString & s0,	// returned only
										   wxString & s1)	// returned only
{
	while (1)
	{
#if defined(__WXMSW__)
		dlg_open_autocomplete dlg(pDialogParent,TBT_FOLDER,pArgs);
#else
		dlg_open dlg(pDialogParent,TBT_FOLDER,pArgs);
#endif
		int result = dlg.ShowModal();
		if (result != wxID_OK)
			return false;		// cancel

		s0 = dlg.getPath(0);
		s1 = dlg.getPath(1);

		pArgs = NULL;	// only preseed with cl_args the first time.

		if (!_checkForUniqueFolders(pDialogParent,s0,s1))
			continue;	// they don't want the same folders in both -- loop again and let them retry
		
		return true;		// unique folders or they want the same in both
	}
}

//////////////////////////////////////////////////////////////////

void FrameFactory::openFileDiffFromDialogs(gui_frame * pDialogParent)
{
	// using the given window as a dialog parent, ask for a set of
	// file pathnames and then open then in a new or recycled window
	// or spawn them in an external tool.

	wxString s0, s1;

	if (!_raiseOpenFileDiffDialog(&pDialogParent,NULL,s0,s1))
		return;			// user cancelled

	openFileDiffFrameOrSpawnAsyncXT(s0,s1,NULL);
}

void FrameFactory::openFileDiffFrameOrSpawnAsyncXT(const wxString & s0, const wxString & s1,
												   const cl_args * pArgs)
{
	// open the given set of files in a DiffMerge window or spawn them
	// async into an external tool.

	const xt_tool * pxt = NULL;
	wxString strXtExe, strXtArgs;

	openFileDiffFrame(s0,s1,pArgs,&pxt,strXtExe,strXtArgs);

	if (pxt)
		gui_app::spawnAsyncXT(pxt,strXtExe,strXtArgs);
}

bool FrameFactory::_checkForUniqueFiles(gui_frame ** ppDialogParent,
										const cl_args * pArgs,	// may be null, only used for caption
										const wxString & s0,	// input only
										const wxString & s1)
{
	// TODO put these pathnames into wxFileName and normalize them and
	// TODO then compare the normalized result -- so that "." and "./"
	// TODO compare equal, for example.

	if (s0.Cmp(s1) != 0)	// different pathnames, accept them.
		return true;
		
	// same pathname for both, ask for confirmation.

	// we pre-test the dont-show-again key to avoid creating
	// a frame window if we already know the answer.
	
	bool bKeyValue = gpGlobalProps->getBool(GlobalProps::GPL_VIEW_FILE_DONT_SHOW_SAME_FILES_MSGBOX);
	if (bKeyValue)			// don't show if already set
		return true;
		
	if (!*ppDialogParent)	// if no parent frame, create one.
		*ppDialogParent = _createFirstFrame(pArgs);
	
	int result = util_dont_show_again_msgbox::msgbox(*ppDialogParent,
													 _("Warning"),
													 wxGetTranslation(L"Both pathnames refer to the same file.\n"
																	  L"\n"
																	  L"Are you sure you want to open this window?"),
													 GlobalProps::GPL_VIEW_FILE_DONT_SHOW_SAME_FILES_MSGBOX,
													 wxYES|wxNO);
	if (result == wxID_OK)	// they really want to do open the same path in both sides
		return true;

	return false;
}

bool FrameFactory::_raiseOpenFileDiffDialog(gui_frame ** ppDialogParent,
											const cl_args * pArgs,
											wxString & s0,	// returned only
											wxString & s1)	// returned only
{
	if (!*ppDialogParent)
		*ppDialogParent = _createFirstFrame(pArgs);

	while (1)
	{
#if defined(__WXMSW__)
		dlg_open_autocomplete dlg(*ppDialogParent,TBT_DIFF,pArgs);
#else
		dlg_open dlg(*ppDialogParent,TBT_DIFF,pArgs);
#endif
		int result = dlg.ShowModal();
		if (result != wxID_OK)
			return false;
	
		s0 = dlg.getPath(0);
		s1 = dlg.getPath(1);

		pArgs = NULL;	// only preseed with cl_args the first time.

		if (!_checkForUniqueFiles(ppDialogParent,pArgs,s0,s1))
			continue;	// they don't want the same files in both -- loop again and let them retry

		// unique files or they want the same in both

		// if s1 contains the name of a temp file (edit buffer) being
		// used by another window, we can really confuse the user --
		// their edits in the other window will change document (SYNC_VIEW,PANEL_T1)
		// in the new window -- not (SYNC_EDIT,PANEL_EDIT) -- so if they
		// then enable-editing in this window, they'll have forked a copy
		// of the document they are editing -- this is probably ***NOT***
		// what they want.  if they try to load a temp file, we ask them
		// if they'd rather refer to the underlying source document and
		// maybe also enable editing in this window -- that way the same
		// edit-buffer will be shared and active in both windows.
		// 
		// FWIW, if they name a edit-buffer temp file for s0, we don't
		// care -- because this is kind of useful and would let them see
		// what their edits in the other window are doing to the comparison
		// of a third/fourth/nth file.

		poi_item * pPoi1 = gpPoiItemTable->addItem(s1);
		poi_item * pPoiSrc = pPoi1->getPoiEditSrc();
		if (!pPoiSrc)
			return true;
	
		result = _ask_about_temp_file_usage(*ppDialogParent,pPoi1);
		switch (result)
		{
		default:
		case wxID_CANCEL:
			break;			// loop again and let them retry

		case wxID_NO:			// let s1 remain the temp buf.
			return true;		// don't say we didn't warn them.

		case wxID_YES:			// substitute the source file 
			s1 = pPoiSrc->getFullPath();
			return true;
		}
	}
}

//////////////////////////////////////////////////////////////////

void FrameFactory::openFileMergeFromDialogs(gui_frame * pDialogParent)
{
	// using the given window as a dialog parent, ask for a set of
	// file pathnames and then open then in a new or recycled window
	// or spawn them in an external tool.

	wxString s0, s1, s2;

	if (!_raiseOpenFileMergeDialog(&pDialogParent,NULL,s0,s1,s2))
		return;

	openFileMergeFrameOrSpawnAsyncXT(s0,s1,s2,NULL);
}

void FrameFactory::openFileMergeFrameOrSpawnAsyncXT(const wxString & s0, const wxString & s1, const wxString & s2,
													const cl_args * pArgs)
{
	// open the given set of files in a DiffMerge window or spawn them
	// async into an external tool.

	const xt_tool * pxt = NULL;
	wxString strXtExe, strXtArgs;

	openFileMergeFrame(s0,s1,s2,pArgs,&pxt,strXtExe,strXtArgs);

	if (pxt)
		gui_app::spawnAsyncXT(pxt,strXtExe,strXtArgs);
}

bool FrameFactory::_checkForUniqueFiles(gui_frame ** ppDialogParent,
										const cl_args * pArgs,	// use for caption only
										const wxString & s0,	// input only
										const wxString & s1,
										const wxString & s2)
{
	// TODO put these pathnames into wxFileName and normalize them and
	// TODO then compare the normalized result -- so that "." and "./"
	// TODO compare equal, for example.

	if ((s0.Cmp(s1) != 0)
		&& (s0.Cmp(s2) != 0)
		&& (s1.Cmp(s2) != 0))
		return true;	// different pathnames, accept them.
		
	// at least one duplicate, ask for confirmation.

	// we pre-test the dont-show-again key to avoid creating
	// a frame window if we already know the answer.
	
	bool bKeyValue = gpGlobalProps->getBool(GlobalProps::GPL_VIEW_FILE_DONT_SHOW_SAME_FILES_MSGBOX);
	if (bKeyValue)			// don't show if already set
		return true;

	if (!*ppDialogParent)
		*ppDialogParent = _createFirstFrame(pArgs);

	int result = util_dont_show_again_msgbox::msgbox(*ppDialogParent,
													 _("Warning"),
													 wxGetTranslation(L"The same file has been chosen more than once.\n"
																	  L"\n"
																	  L"Are you sure you want to open this window?"),
													 GlobalProps::GPL_VIEW_FILE_DONT_SHOW_SAME_FILES_MSGBOX,
													 wxYES|wxNO);
	if (result == wxID_OK)	// they really want to do open the same path in more than one panel
		return true;

	return false;
}

bool FrameFactory::_raiseOpenFileMergeDialog(gui_frame ** ppDialogParent,
											 const cl_args * pArgs,
											 wxString & s0,	// returned only
											 wxString & s1,	// returned only
											 wxString & s2)	// returned only
{
	if (!*ppDialogParent)
		*ppDialogParent = _createFirstFrame(pArgs);

	while (1)
	{
#if defined(__WXMSW__)
		dlg_open_autocomplete dlg(*ppDialogParent,TBT_MERGE,pArgs);
#else
		dlg_open dlg(*ppDialogParent,TBT_MERGE,pArgs);
#endif
		int result = dlg.ShowModal();
		if (result != wxID_OK)
			return false;

		s0 = dlg.getPath(0);
		s1 = dlg.getPath(1);
		s2 = dlg.getPath(2);

		pArgs = NULL;	// only preseed with cl_args the first time.

		if (!_checkForUniqueFiles(ppDialogParent,pArgs,s0,s1,s2))
			continue;	// they don't want the duplicate files -- loop again and let them retry

		// unique files or they want duplicates

		// see note in openFileDiffFromDialogs about naming a temp file.

		poi_item * pPoi1 = gpPoiItemTable->addItem(s1);
		poi_item * pPoiSrc = pPoi1->getPoiEditSrc();
		if (!pPoiSrc)
			return true;

		result = _ask_about_temp_file_usage(*ppDialogParent,pPoi1);
		switch (result)
		{
		default:
		case wxID_CANCEL:
			break;			// loop again and let them retry

		case wxID_NO:		// let s1 remain the temp buf.
			return true;	// don't say we didn't warn them.

		case wxID_YES:		// substitute the source file 
			s1 = pPoiSrc->getFullPath();
			return true;
		}
	}
}

//////////////////////////////////////////////////////////////////

void FrameFactory::raiseFrame(gui_frame * pFrame) const
{
	// raise the given frame and make it the active window.
	// TODO make this work correctly on all platforms.
	
#if defined(__WXMAC__)
	// WXBUG if this call came from a double-click on a folder row in an
	// WXBUG existing folderdiff window, the raise won't have any effect
	// WXBUG on the mac -- or rather, the window they clicked in will still
	// WXBUG be on top.  looks like an issue with the macs focus model.
	// WXBUG however, it does work when they use the toolbar button.
#endif
#if defined(__WXGTK__)
	// WXBUG on GTK the raise works (from either toolbar/menu or double-click
	// WXBUG on row), but the window does not get activated (probably a
	// WXBUG WindowManager issue).
#endif
#if defined(__WXMSW__)
	// on Win32, the Raise() is sufficient to raise and activate the window.
#endif
	pFrame->Raise();
//	pFrame->SetFocus();  // this didn't help on gtk
}

//////////////////////////////////////////////////////////////////

gui_frame * FrameFactory::findFolderFrame(const fd_fd * pFdFd)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	// find first (should be only) frame that claims to be a folder-diff with the given folder pair

	for (TMapConstIterator cit=m_map.begin(); (cit!=m_map.end()); cit++)
	{
		gui_frame * pFrame = cit->second;
		if (pFrame->isFolder(pFdFd))
			return pFrame;
	}

	return NULL;
}

gui_frame * FrameFactory::findFileDiffFrame(const fs_fs * pFsFs)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	// find first frame that claims to be a file-diff with the given file pair

	for (TMapConstIterator cit=m_map.begin(); (cit!=m_map.end()); cit++)
	{
		gui_frame * pFrame = cit->second;
		if (pFrame->isFileDiff(pFsFs))
			return pFrame;
	}

	return NULL;
}

gui_frame * FrameFactory::findFileMergeFrame(const fs_fs * pFsFs)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	// find first frame that claims to be a file-merge with the given file set.

	for (TMapConstIterator cit=m_map.begin(); (cit!=m_map.end()); cit++)
	{
		gui_frame * pFrame = cit->second;
		if (pFrame->isFileMerge(pFsFs))
			return pFrame;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////

gui_frame * FrameFactory::findEmptyFrame(void)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	// find an empty frame (if we have one).  normally, this can only
	// happen at startup where we create an initial window that is un-typed,
	// so we should have 0 or 1 of these.

	for (TMapConstIterator cit=m_map.begin(); (cit!=m_map.end()); cit++)
	{
		gui_frame * pFrame = cit->second;
		if (pFrame->isEmptyFrame())
			return pFrame;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////

bool FrameFactory::closeAllFrames(bool bForce)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	bool bClosed = true;
	
	while (bClosed && (m_map.begin() != m_map.end()))
	{
		TMapIterator it = m_map.begin();
		gui_frame * pFrame = it->second;
		bClosed = pFrame->Close(bForce);
	}

	return bClosed;
}

//////////////////////////////////////////////////////////////////

void FrameFactory::unlinkFrame(gui_frame * pFrame)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	// remove the frame from the map.
	// WE DO NOT DELETE the frame because we
	// do not own it -- wxWidgets does.

	TMapIterator it = m_map.find(pFrame);
	if (it != m_map.end())
		m_map.erase(it);
}

//////////////////////////////////////////////////////////////////

gui_frame * FrameFactory::findFirstEditableFileFrame(void)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	for (TMapConstIterator cit=m_map.begin(); (cit!=m_map.end()); cit++)
	{
		gui_frame * pFrame = cit->second;
		if (pFrame->isFileDiff() || pFrame->isFileMerge())
		{
			if (pFrame->isEditableFileFrame())
				return pFrame;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////

void FrameFactory::saveAll(wxCommandEvent & eFoo)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	for (TMapConstIterator cit=m_map.begin(); (cit!=m_map.end()); cit++)
	{
		gui_frame * pFrame = cit->second;

		raiseFrame(pFrame);
		pFrame->onFileSave(eFoo);
	}
}

//////////////////////////////////////////////////////////////////

/*static*/ int FrameFactory::_ask_about_temp_file_usage(wxFrame * pDialogParent, poi_item * pPoiEditBuffer)
{
	poi_item * pPoiEditSrc = pPoiEditBuffer->getPoiEditSrc();

	wxString strMessage = wxString::Format( wxGetTranslation(L"The second file that you selected is being used as\n"
															 L"temporary storage for a file being edited in another\n"
															 L"window.\n"
															 L"\n"
															 L"Temporary File: %s\n"
															 L"Source File: %s\n"
															 L"\n"
															 L"Is is highly recommended that you use the source file\n"
															 L"instead.  This will allow the edit buffer to be shared\n"
															 L"between the two windows.  If not, your edit buffer will\n"
															 L"be forked.\n"
															 L"\n"
															 L"Do you want to use the source file instead?"),
											pPoiEditBuffer->getFullPath().wc_str(),
											pPoiEditSrc->getFullPath().wc_str());

	wxMessageDialog dlg(pDialogParent,strMessage,_("Warning: Temporary File Chosen"),
						wxYES_NO|wxCANCEL|wxYES_DEFAULT|wxICON_QUESTION);
	int result = dlg.ShowModal();

	return result;
}

//////////////////////////////////////////////////////////////////

int FrameFactory::countFrames(void)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);
	return (int)m_map.size();
}

// Get the first frame in the map.  We don't care
// what type it is.
gui_frame * FrameFactory::getFirstFrame(void)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	gui_frame * pFrameAny = NULL;
	if (countFrames() > 0)
	{
		TMapConstIterator cit = m_map.begin();
		pFrameAny = cit->second;
	}

	return pFrameAny;
}

//////////////////////////////////////////////////////////////////

bool FrameFactory::haveFrameAtPosition(const wxPoint & pos)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	for (TMapConstIterator cit=m_map.begin(); (cit!=m_map.end()); cit++)
	{
		gui_frame * pFrame = cit->second;

		wxPoint posFrame = pFrame->getRestoredPosition();

		if (posFrame == pos)
		{
//			wxLogTrace(wxTRACE_Messages,_T("FrameFactory:haveFrameAtPosition: [x %d][y %d]"),
//					   pos.x,pos.y);
			return true;
		}
	}

	return false;
}


//////////////////////////////////////////////////////////////////

wxString FrameFactory::dumpSupportInfoFF(void)
{
	// this lock will be around until 'lock' goes out of scope
	wxCriticalSectionLocker lock(m_cs_map);

	wxString str;
	wxString strIndent = _T("\t");

	str += wxString::Format(_T("All Windows:\n"));

	for (TMapConstIterator cit=m_map.begin(); (cit!=m_map.end()); cit++)
	{
		gui_frame * pFrame = cit->second;

		str += pFrame->dumpSupportInfo(strIndent);
	}

	str += _T("\n");

	return str;
}

//////////////////////////////////////////////////////////////////

/*static*/ void FrameFactory::getExternalToolArgs(int nrParams,
												  const wxString & s0,
												  const wxString & s1,
												  const wxString & s2,
												  const cl_args * pArgs,	// only used for titles, result path -- may be null
												  const xt_tool * pxt,
												  wxString & rStrExe,
												  wxString & rStrArgs)
{
	// warning: because we may have gotten here via the open-interlude
	// dialog (which may or may not have been seeded with pathnames
	// from the cl-args), the pathnames in the cl-args may be stale
	// (the user may have updated them in the open-interlude).  so, we
	// always use the s[012] pathnames rather than the values in the
	// cl-args.
	//
	// also, pArgs may be null -- if we were called by the open-interlude
	// or if the user double-clicked on a pair of files in a folder window,
	// for example.

	// lookup the EXE pathname and the command line arg-template for
	// for the given tool.  apply the args we were given on our
	// command line (in cl_args or via the open-interlude dialog) to
	// the template.

	// template pattern substitution is a little different when we have
	// a cl-args.  because we may have both pathnames and titles for each
	// positional file.

	// TitleOrPath()
	// if we were given a title for panel[k], use it.
	// otherwise, use the pathname of the file.  for
	// sgdm, this is kind of arbitrary (because we
	// will do the right thing without the /title
	// argument, but other apps may be more picky).

#define TitleOrPath(kPanel,sk)	((pArgs && pArgs->bTitle[(kPanel)])		\
								 ? pArgs->title[(kPanel)]				\
								 : (sk))

	// ResultOrAncestor()
	// the merge result output file is either explicitly named
	// with the /result= arg or we should overwrite the common
	// ancestor that was used as input.

#define ResultOrAncestor()	((pArgs && pArgs->bResult)					\
							 ? pArgs->result							\
							 : s1)


	// TitleOrResult()
	// for the title of the center panel we want to use the /title label
	// if we have it.  if not, we use the pathname of the result file if
	// given.  if not, we use the pathname of the baseline (ancestor) file.

#define TitleOrResult()		((pArgs && pArgs->bTitle[PANEL_T1])			\
							 ? pArgs->title[PANEL_T1]					\
							 : ResultOrAncestor())

	// make a copy of the correct arg template and
	// then populate it with values from the cl_args.

	if (nrParams==3)
	{
		rStrExe  = pxt->getGui3Exe();
		rStrArgs = pxt->getGui3Args();

		xt_tool::apply_working_path(rStrArgs, s0);
		xt_tool::apply_baseline_path(rStrArgs, s1);
		xt_tool::apply_other_path(rStrArgs, s2);

		xt_tool::apply_destination_path(rStrArgs, ResultOrAncestor());

		xt_tool::apply_working_title(rStrArgs, TitleOrPath(PANEL_T0,s0));
		xt_tool::apply_destination_title(rStrArgs, TitleOrResult());
		xt_tool::apply_other_title(rStrArgs, TitleOrPath(PANEL_T2,s2));

		// TODO do we want to pass the /ro1, /ro3 flags???
	}
	else
	{
		rStrExe  = pxt->getGui2Exe();
		rStrArgs = pxt->getGui2Args();

		xt_tool::apply_left_path(rStrArgs, s0);
		xt_tool::apply_right_path(rStrArgs, s1);

		xt_tool::apply_left_title(rStrArgs, TitleOrPath(PANEL_T0,s0));
		xt_tool::apply_right_title(rStrArgs, TitleOrPath(PANEL_T1,s1));

		// TODO do we want to pass the /ro1, /ro2 flags???
		// TODO do we want to pass the /merge flags???
	}

	// TODO do we want to pass the caption???
}

/*static*/ bool FrameFactory::lookupExternalTool(int nrParams,
												 const wxString & s0,
												 const wxString & s1,
												 const wxString & s2,
												 const cl_args * pArgs,	// only used for titles, result path -- may be null
												 const xt_tool ** ppxt,
												 wxString & rStrXtExe,
												 wxString & rStrXtArgs)
{
	// use file suffixes on the given files to look up an external tool.
	// we now also include the result-path, if given, when searching for
	// suffixes.  see item:13394.

	// return true and fill in *ppxt,rStrXtExe,rStrXtArgs if one is
	// selected (and the user wants to use it).
	// return false if no match (or user doesn't want to use it).

	const xt_tool * pxt;
	if ((nrParams==3) && pArgs && pArgs->bResult)
		pxt = gpXtToolTable->findExternalTool(s0,s1,s2,pArgs->result);
	else
		pxt = gpXtToolTable->findExternalTool(nrParams,s0,s1,s2);
	if (!pxt)
		return false;
	
	wxString strXtExe, strXtArgs;
	FrameFactory::getExternalToolArgs(nrParams,s0,s1,s2,pArgs,pxt,strXtExe,strXtArgs);

	wxString strMsg;
	strMsg += _("DiffMerge has selected external tool '");
	strMsg += pxt->getName().wc_str();
	strMsg += _("' for files of this type.\n");
	strMsg += _T("\n");
	strMsg += strXtExe;		strMsg += _T("\n");
	strMsg += strXtArgs;	strMsg += _T("\n");
	strMsg += _T("\n");
	strMsg += _("Do you want to use this tool?  (Select NO to open with DiffMerge.)\n");
	
//	wxLogTrace(TRACE_XT_DUMP,_T("%s"),strMsg.wc_str());

	// TODO we didn't get passed a parent window for this dialog.
	// TODO i don't want to force create one if the dialog box isn't
	// TODO going to be used.  revisit this if it becomes a problem.
	int result = util_dont_show_again_msgbox::msgbox(NULL,
													 VER_APP_TITLE,
													 strMsg,
													 GlobalProps::GPL_EXTERNAL_TOOLS_DONT_SHOW_EXEC_MSG,
													 wxYES|wxNO);
	if (result != wxID_OK)
		return false;
	
	*ppxt = pxt;
	rStrXtExe = strXtExe;
	rStrXtArgs = strXtArgs;
	return true;
}

//////////////////////////////////////////////////////////////////

void FrameFactory::onMenuFileExit(wxFrame * pFrame)
{
//	wxLogTrace(wxTRACE_Messages, _T("onMenuFileExit"));

	// close all windows, starting with the given window.
	// if multiple windows open, ask for confirmation.
	// this count only includes gui_frames (not dlg_nag frames).

	int count = countFrames();
	if (count > 1)
	{
		wxString strMsg = wxString::Format(wxGetTranslation(L"There are %d windows open.\n"
															L"\n"
															L"Are you sure you want to close all windows and exit?"),
										   count);

		int result = util_dont_show_again_msgbox::msgbox(pFrame,
														 _("Warning"),
														 strMsg,
														 GlobalProps::GPL_VIEW_FILE_DONT_SHOW_EXIT_MULTIPLE_WINDOWS_MSGBOX,
														 wxYES|wxNO);
		if (result != wxID_OK)
			return;
	}

	bool bForce = false;					// do not force us to close
	bool bClosed = pFrame->Close(bForce);	// send wxCloseEvent to this window first
	if (bClosed)
	{
		pFrame = NULL;	// if it actually closed, pFrame may have been deleted on us.
		
		// cause wxCloseEvent to go to all other windows
		bClosed = gpFrameFactory->closeAllFrames(bForce);
	}

//	wxLogTrace(wxTRACE_Messages, _T("onMenuFileExit: [closed %d]"), bClosed);
}

//////////////////////////////////////////////////////////////////

static util_error _determine_type(const wxArrayString & asFilenames,
								  POI_TYPE * pPoiType)
{
	util_error ue;

	int nrFiles = 0;
	int nrFolders = 0;
	int kLimit = asFilenames.GetCount();
	int k;

	if (kLimit > 2)		// ignore the rest
		kLimit = 2;
	
	for (k=0; k<kLimit; k++)
	{
		poi_item * pPoi_k = NULL;
		wxString str_k = asFilenames[k];

#if defined(__WXMAC__) || defined(__WXGTK__)
	deref:
#endif
		pPoi_k = gpPoiItemTable->addItem(str_k);
		switch (pPoi_k->getPoiType())
		{
		case POI_T_FILE:
			nrFiles++;
			break;

		case POI_T_DIR:
			nrFolders++;
			break;

#if defined(__WXMAC__) || defined(__WXGTK__)
		case POI_T_SYMLINK:
			ue = pPoi_k->get_symlink_target(str_k);
			if (ue.isErr())
				return ue;
			goto deref;
#endif

		default:
			// not found or a device special file or who knows what.
			ue.set(util_error::UE_INVALID_PATHNAME, str_k);
			return ue;
		}
	}

	if ((nrFiles > 0) && (nrFolders > 0))
	{
		// Finder shouldn't do this (unless they use the "Finder | Services" menu).
		// Just give up.
		// TODO 2013/08/08 It'd be nice to be able to try to do something like
		// TODO            look inside the folder and see if there is a file
		// TODO            with the same filename as the other and *assume*
		// TODO            they meant that -- kinda like gnudiff does.  *BUT*
		// TODO            it is not clear that that is necessary considering
		// TODO            how the user has to multi-select items in Finder.
		ue.set(util_error::UE_CANNOT_COMPARE_ITEMS,
			   _T("A file and a folder."));
		return ue;
	}

	if (nrFiles)
		*pPoiType = POI_T_FILE;
	else
		*pPoiType = POI_T_DIR;

	return ue;
}

#if defined(__WXMAC__)
// Finder Integration.  Respond to request from Finder to open a set of files
// or folders.  We can get called in 2 contexts:
// 
// [1] if we were already running, Finder will ask us to open another window.
// [2] if we not already running, Finder will start us with no command-line arguments
//     and then send us the set of items.  (unlike Windows Explorer Integration.)
//
// In case [1] we may or may not have the original empty frame window up
// (the first successful set of items repurposes the intial empty frame).
// In case [2] we should have the original empty frame window up.
//     
// Also different from Windows is that we can get any number of items.
// Arbitrary decision: we'll only look at the first 2 items and ignore the rest.
// Arbitrary decision: we'll always force the dlg_open interlude so that they
// can see the order of the items (even if they give us 2 exactly).
//
// So we want to either create a new window or re-purpose the initial Empty frame.
// We want to behave a lot like openFirstFrame().
//
// We DO NOT set the app's exit status because we don't want to mess up an
// on-going merge (in the case when we were already running and started by
// a VCS tool before Finder sent us this set of items).
//
// Note that if the pair that we get back from dlg_open is already open in a
// window, we just raise it (like we always do in all other cases) rather than
// creating a new/duplicate window.
#endif
// We are also used for Drag-n-Drop support.  I've made the whole gui_frame
// a DropTarget so that users can drop 1 or 2 items anywhere on a window an
// we will try to open a new window (using dlg_open to ensure order).
//
// pFrame is optional. it should be set by DND so that we
// properly parent the dlg_open.  If we're coming from Finder
// and don't have an obvious parent, we grab one at random.
// (Not sure how this could happen, but if there are no frames,
// we'll create an EmptyFrame.)
void FrameFactory::openFrameRequestedByFinderOrDND(gui_frame * pFrame,
												   const wxArrayString & asFilenames)
{
	util_error ue;
	POI_TYPE poiType;

	int kLimit = asFilenames.GetCount();
	if (kLimit == 0)	// should not happen
		return;

	if (!pFrame)
		pFrame = getFirstFrame();

	ue = _determine_type(asFilenames, &poiType);
	if (ue.isErr())
	{
		wxMessageDialog dlg(pFrame, ue.getMBMessage().wc_str(),
							VER_APP_TITLE, wxOK|wxICON_ERROR);
		dlg.ShowModal();
		return;
	}

	// fake up a set of CL_ARGS with just enough stuff
	// to let me re-use some code.
	cl_args args;
	args.bFolders = (poiType == POI_T_DIR);
	args.nrParams = kLimit;
	args.pathname[0] = asFilenames[0];
	if (kLimit > 1)
		args.pathname[1] = asFilenames[1];

	wxString s0, s1;
	if (args.bFolders)
	{
		if (_raiseOpenFoldersDialog(pFrame, &args, s0, s1))
			openFolderFrame(s0, s1, &args);
		return;
	}
	else
	{
		if (_raiseOpenFileDiffDialog(&pFrame, &args, s0, s1))
			openFileDiffFrameOrSpawnAsyncXT(s0, s1, &args);
		return;
	}
	
}

