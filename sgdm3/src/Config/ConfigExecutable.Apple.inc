## Config/ConfigExecutable.Apple.inc -- GNU Makefile snippets for
## building an executable.
##################################################################

## we create a diskimage directory.
## we create a bundle (DiffMerge.app) within it and populate it with
## bundle structure and static and versioned content, and compile/link
## the .exe inplace.
## we populate the disk image directory and package it.

ifeq ($(strip $(BUILD_BUILDLABEL)),)
DOT_BUILDLABEL:=
else
DOT_BUILDLABEL:=.$(BUILD_BUILDLABEL)
endif

## on Apple, the package is called "DiffMerge 3.0.0". it contains an
## application named "DiffMerge.app" which contains an executable
## named "DiffMerge".

PACKAGE:=	DiffMerge
TARGET_EXE:=	DiffMerge

BASE_PATH:= $(BUILD)/$(OS_NAME)/$(ARCH)/$(MODE)/tmp/$(BUILD_MAJOR).$(BUILD_MINOR).$(BUILD_REVISION).$(BUILD_BUILDNUM)$(DOT_BUILDLABEL)
DMGDIR:=	$(BASE_PATH)/dmg

BUNDLE:=	$(BASE_PATH)/$(TARGET_EXE).app
BINDIR:=	$(BUNDLE)/Contents/MacOS
TARGET:=	$(BINDIR)/$(TARGET_EXE)$(EXE_SFX)
BUILD_VERSION:= $(BUILD_MAJOR).$(BUILD_MINOR).$(BUILD_REVISION).$(BUILD_BUILDNUM)

ifeq ($(strip $(BUILD_BUILDLABEL)),)
DMGDIR2:=	"$(PACKAGE) $(BUILD_MAJOR).$(BUILD_MINOR).$(BUILD_REVISION).$(BUILD_BUILDNUM) $(ARCH_PACKAGE)"
else
DMGDIR2:=	"$(PACKAGE) $(BUILD_MAJOR).$(BUILD_MINOR).$(BUILD_REVISION).$(BUILD_BUILDNUM) $(ARCH_PACKAGE) $(BUILD_BUILDLABEL)"
endif

DMG:=		$(BUILD)/$(OS_NAME)/$(ARCH)/$(MODE)/$(PACKAGE).$(BUILD_MAJOR).$(BUILD_MINOR).$(BUILD_REVISION).$(BUILD_BUILDNUM).$(ARCH_PACKAGE)$(DOT_BUILDLABEL).dmg

MY_LD_FLAGS+=	-sectcreate __TEXT __info_plist $(BUNDLE)/Contents/Info.plist

OSX_RELEASE:=$(shell uname -v)

##################################################################

ifeq ($(MODE),Release)
default:	$(DMG)
else
default:	$(TARGET)
endif

##################################################################

include ../Config/ConfigCompile.inc

##################################################################
## build bundle structure and copy static and versioned files into it.

APP_DIRS:=	$(BUNDLE) \
		$(BUNDLE)/Contents \
		$(BUNDLE)/Contents/MacOS \
		$(BUNDLE)/Contents/Resources


bundle_setup:
	@echo Setting up bundle structure $(BUNDLE)
	@#
	@#
	@for dir in $(APP_DIRS); do \
	     [ -d $$dir ] || mkdir -p $$dir; \
	done
	@#
	@#
	@cp ../gui/Artwork/DiffMergeIcon.icns $(BUNDLE)/Contents/Resources/
	@chmod 644 $(BUNDLE)/Contents/Resources/DiffMergeIcon.icns
	@#
	@#
	@cp ../Installers/DiffMergeManual.pdf $(BUNDLE)/Contents/Resources/DiffMergeManual.pdf
	@chmod 644 $(BUNDLE)/Contents/Resources/DiffMergeManual.pdf
	@#
	@# Hide a copy of the shell script inside the APP bundle so that
	@# other apps (like Veracity) can launch us even if the user just
	@# does the minimal install.
	@#
	@cat ../Installers/$(OS_NAME).CommandLine.sh		\
	| sed 's|@MAJOR@|$(BUILD_MAJOR)|'			\
	| sed 's|@MINOR@|$(BUILD_MINOR)|'			\
	| sed 's|@REVISION@|$(BUILD_REVISION)|'			\
	| sed 's|@BUILDNUM@|$(BUILD_BUILDNUM)|'			\
	| sed 's|@COPYRIGHTDATE@|$(BUILD_COPYRIGHTDATE)|'	\
	| sed 's|@BUILDLABEL@|$(BUILD_BUILDLABEL)|'		\
	| sed 's|@CSID@|$(BUILD_CSID)|g'			\
	> $(BUNDLE)/Contents/Resources/diffmerge.sh
	@chmod 755 $(BUNDLE)/Contents/Resources/diffmerge.sh
	@#
	@#
	@cat ../Installers/Apple.Info.plist			\
	| sed 's|@MAJOR@|$(BUILD_MAJOR)|'			\
	| sed 's|@ARCH@|$(ARCH)|'			\
	| sed 's|@MINOR@|$(BUILD_MINOR)|'			\
	| sed 's|@REVISION@|$(BUILD_REVISION)|'			\
	| sed 's|@BUILDNUM@|$(BUILD_BUILDNUM)|'			\
	| sed 's|@COPYRIGHTDATE@|$(BUILD_COPYRIGHTDATE)|'	\
	| sed 's|@BUILDLABEL@|$(BUILD_BUILDLABEL)|'		\
	| sed 's|@CSID@|$(BUILD_CSID)|g'			\
	> $(BUNDLE)/Contents/Info.plist
	@chmod 644 $(BUNDLE)/Contents/Info.plist

##################################################################

MY_EXEDEPS+=	../Config/ConfigExecutable.$(OS_NAME).inc

$(TARGET):	bundle_setup $(OBJS) $(MY_EXEDEPS)
	@[ -d $(BINDIR) ] || mkdir -p $(BINDIR)
	@echo Linking $@
	@echo
	$(WX_CXX) -o $(TARGET) $(OBJS) $(MY_LD_FLAGS) $(MY_LD_LIBS) $(WX_CXX_LIBS)
	@echo
ifeq ($(MODE),Release)
	@strip $(TARGET)
endif
	@SetFile -t APPL $(TARGET)
	@#
	@# codesign refuses to sign anything carrying a resource fork or
	@# Finder Info (e.g. the FinderInfo xattr that SetFile just added,
	@# or detritus left over from copying the .icns/.pdf into the
	@# bundle), so strip all extended attributes before signing.
	@xattr -cr $(BUNDLE)
	@#
	@# Ad-hoc code sign the bundle so it satisfies the Apple Silicon
	@# "must be signed to run" requirement. This is NOT a Developer ID
	@# signature, so Gatekeeper will still flag the app as being from
	@# an unidentified developer on other Macs (the quarantine flag
	@# still needs to be cleared, or the user must right-click > Open).
	@echo Ad-hoc signing $(BUNDLE)
	codesign --force --deep -s - $(BUNDLE)


##################################################################
## build diskimage structure -- the bundle, extra copy of manual, and a readme.

## we only use EXE_SH to refer to the example shell script that we included.
## this assumes that the user will install the shell script as /usr/bin/diffmerge

EXE_SH:=	diffmerge

diskimage_setup: $(TARGET)
	@echo
	@echo ================================================================
	@echo
	@echo Setting up diskimage folder $(DMGDIR).
	@#
	@#
	@rm -rf $(DMGDIR)
	@mkdir $(DMGDIR)
	@mkdir $(DMGDIR)/Extras
	@#
	@#
	@cp -r $(BUNDLE) $(DMGDIR)/DiffMerge.app
	@#
	@#
	@ln -s /Applications $(DMGDIR)/Applications
	@#
	@#
	@cp ../Installers/$(OS_NAME).DMG.VolumeIcon.icns $(DMGDIR)/.VolumeIcon.icns
	@SetFile -c icnC $(DMGDIR)/.VolumeIcon.icns
	@chmod 644 $(DMGDIR)/.VolumeIcon.icns
	@#
	@#
	@# Put a hard-link to the manual in the Extras directory.
	@ln $(DMGDIR)/DiffMerge.app/Contents/Resources/DiffMergeManual.pdf $(DMGDIR)/Extras/DiffMergeManual.pdf
	@#
	@#
	@cat ../Installers/$(OS_NAME).Readme.txt		\
	| sed 's|@MAJOR@|$(BUILD_MAJOR)|'			\
	| sed 's|@MINOR@|$(BUILD_MINOR)|'			\
	| sed 's|@REVISION@|$(BUILD_REVISION)|'			\
	| sed 's|@BUILDNUM@|$(BUILD_BUILDNUM)|'			\
	| sed 's|@COPYRIGHTDATE@|$(BUILD_COPYRIGHTDATE)|'	\
	| sed 's|@BUILDLABEL@|$(BUILD_BUILDLABEL)|'		\
	| sed 's|@CSID@|$(BUILD_CSID)|g'			\
	| sed 's|@OSX_RELEASE@|$(OSX_RELEASE)|'			\
	> $(DMGDIR)/Extras/Readme.txt
	@chmod 644 $(DMGDIR)/Extras/Readme.txt
	@#
	@#
	@cat ../Installers/$(OS_NAME).CommandLine.sh		\
	| sed 's|@MAJOR@|$(BUILD_MAJOR)|'			\
	| sed 's|@MINOR@|$(BUILD_MINOR)|'			\
	| sed 's|@REVISION@|$(BUILD_REVISION)|'			\
	| sed 's|@BUILDNUM@|$(BUILD_BUILDNUM)|'			\
	| sed 's|@COPYRIGHTDATE@|$(BUILD_COPYRIGHTDATE)|'	\
	| sed 's|@BUILDLABEL@|$(BUILD_BUILDLABEL)|'		\
	| sed 's|@CSID@|$(BUILD_CSID)|g'			\
	> $(DMGDIR)/Extras/diffmerge.sh
	@chmod 755 $(DMGDIR)/Extras/diffmerge.sh
	@#
	@#
	@cat ../Installers/$(OS_NAME).man1			\
	| sed 's|@MAJOR@|$(BUILD_MAJOR)|'			\
	| sed 's|@MINOR@|$(BUILD_MINOR)|'			\
	| sed 's|@REVISION@|$(BUILD_REVISION)|'			\
	| sed 's|@BUILDNUM@|$(BUILD_BUILDNUM)|'			\
	| sed 's|@COPYRIGHTDATE@|$(BUILD_COPYRIGHTDATE)|'	\
	| sed 's|@BUILDLABEL@|$(BUILD_BUILDLABEL)|'		\
	| sed 's|@CSID@|$(BUILD_CSID)|g'			\
	| sed 's|@PACKAGE@|$(PACKAGE)|'				\
	| sed 's|@EXE@|$(EXE_SH)|'				\
	> $(DMGDIR)/Extras/diffmerge.1


##################################################################
## convert dmg directory to a .dmg file.
##
## to set the VolumeIcon, we must create the DMG as a READ-WRITE
## volume, mount it, set properties within the directory,
## then unmount it, and convert it to a READ-ONLY DMG.
## for more info on the hurdles, see:
##
## http://www.rkuntz.org/pmwiki.php?n=Code.AntDiskImage
## http://endrift.com/blog/2010/06/14/dmg-files-volume-icons-cli/
## https://github.com/GPGTools/GPGTools_Core/blob/master/scripts/create_dmg.sh

$(DMG):	diskimage_setup
	@echo
	@echo ================================================================
	@echo
	@echo Creating $(DMG)
	@if [ -f $(DMG) ] ; then rm $(DMG); fi
	@#
	hdiutil create -srcfolder $(BASE_PATH)/dmg -volname $(DMGDIR2) -format UDRW -ov $(BASE_PATH)/raw-dmg.dmg
	rm -rf $(BASE_PATH)/mnt
	mkdir $(BASE_PATH)/mnt
	hdiutil attach $(BASE_PATH)/raw-dmg.dmg -mountpoint $(BASE_PATH)/mnt
	SetFile -a C $(BASE_PATH)/mnt
	@if [ -d "$(BASE_PATH)/mnt/.Trashes" ]; then   \
		chmod 755 $(BASE_PATH)/mnt/.Trashes;   \
		rm -r $(BASE_PATH)/mnt/.Trashes;       \
	fi
	@if [ -d "$(BASE_PATH)/mnt/.fseventsd" ]; then \
		rm -r $(BASE_PATH)/mnt/.fseventsd;     \
	fi
	hdiutil detach $(BASE_PATH)/mnt
	rm -rf $(BASE_PATH)/mnt
	@#
	@# due to an issue in OSX, raw-dmg.dmg still has an open file handle 
	@# after calling hdiutil detach or there was a change to hdiutil
	@# that now requires a file to be "converted" NOT be opened
	@# by anything else, the raw-dmg file will be copied to raw-dmg-cp.dmg,
	@# and that copy will be used with htidutil's convert.
	cp -f -p $(BASE_PATH)/raw-dmg.dmg $(BASE_PATH)/raw-dmg-cp.dmg
	@#
	hdiutil convert $(BASE_PATH)/raw-dmg-cp.dmg -format UDZO -o $(DMG)
	rm -rf $(BASE_PATH)/raw-dmg.dmg
	rm -rf $(BASE_PATH)/raw-dmg-cp.dmg

##################################################################

clean:
	@rm -f $(OBJS)

##################################################################

.PHONY:	clean run bundle_setup diskimage_setup

##################################################################
## describe the outputs

report_outputs:
	@echo $(DMG)

