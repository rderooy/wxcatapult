# $Id$
#
# Write build info to C++ constants, so source can access it.
# Advantages of this approach:
# - file dates used for dependency checks (as opposed to "-D" compile flag)
# - inactive code is still checked by compiler (as opposed to "#if")

$(CONFIG_HEADER): $(MAKE_PATH)/info2code.mk $(CONFIG_MAKEFILE)
	@echo "Creating $@..."
	@mkdir -p $(@D)
	@echo "// Automatically generated by build process." > $@
	@echo "" >> $@
	@echo "#ifndef __CONFIG_H__" >> $@
	@echo "#define __CONFIG_H__" >> $@
	@echo "" >> $@
	@echo "#ifndef WX_PRECOMP" >> $@
	@echo "#include <wx/wx.h>" >> $@
	@echo "#endif" >> $@
	@echo "" >> $@
	@echo "static const wxString RESOURCEDIR = \"$(INSTALL_BASE)/resources\";" >> $@
	@echo "" >> $@
	@echo "#endif //__CONFIG_H__" >> $@

$(VERSION_HEADER): ChangeLog $(MAKE_PATH)/info2code.mk $(MAKE_PATH)/version.mk
	@echo "Creating $@..."
	@mkdir -p $(@D)
	@echo "// Automatically generated by build process." > $@
	@echo "" >> $@
	@echo "const bool Version::RELEASE = $(RELEASE_FLAG);" >> $@
	@echo "const wxString Version::VERSION = \"$(PACKAGE_VERSION)\";" >> $@
	@echo "const wxString Version::CHANGELOG_REVISION = \"$(CHANGELOG_REVISION)\";" >> $@

