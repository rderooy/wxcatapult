# $Id$
# Generates version include file.

from outpututils import rewriteIfChanged
from version import extractRevision, packageVersion, releaseFlag

import sys

def iterVersionInclude():
	revision = extractRevision()

	yield '// Automatically generated by build process.'
	yield 'const bool Version::RELEASE = %s;' % str(releaseFlag).lower()
	yield 'const wxString Version::VERSION = wxT("%s");' % packageVersion
	yield 'const wxString Version::CHANGELOG_REVISION = wxT("%s");' % revision

if __name__ == '__main__':
	if len(sys.argv) == 2:
		rewriteIfChanged(sys.argv[1], iterVersionInclude())
	else:
		print >> sys.stderr, \
			'Usage: python version2code.py VERSION_HEADER'
		sys.exit(2)
