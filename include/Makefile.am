if LTTNG_TOOLS_BUILD_GIT_SOURCE
GIT_DESCRIBE_CMD = (cd $(top_srcdir); git describe)
else
GIT_DESCRIBE_CMD = /bin/true
endif

##
## The version.h file must be verified and generated or updated if the
## git commit id (called git version here) changed since the last build
## of lttng-tools.
##
version.h:
	##
	## We first create variables for the current git version and
	## the locations of the version.h and version.h.tmpl files.
	##
	@echo -n "Generating version.h... "
	@(version_h_tmpl="$(top_srcdir)/include/version.h.tmpl"; \
	if [ -f "$${version_h_tmpl}" ]; then \
		version_h="$(top_builddir)/include/version.h"; \
		##
		## Check whether we are in a git repo.
		##
		git_describe="$$($(GIT_DESCRIBE_CMD) 2>/dev/null)"; \
		if [ $$? -eq 0 ]; then \
			git_version="$${git_describe}"; \
		else \
			git_version=""; \
		fi; \
		##
		## If the version.h file doesn't exist or is not up to date,
		## We replace it by the version.h.tmpl file.
		##
		if [ ! -e "$${version_h}" ] || \
			[ "$${version_h_tmpl}" -nt "$${version_h}" ]; then \
			cp "$${version_h_tmpl}" "$${version_h}"; \
		fi; \
		echo -n "git version: \"$${git_version}\""; \
		##
		## We verify that git_version isn't the same as the one
		## currently in the file (if there is one), as we don't
		## want to update the file if it is already up to date.
		##
		version_match='^#define GIT_VERSION.*'; \
		old_version=$$(grep "$${version_match}" "$${version_h}"); \
		new_version="#define GIT_VERSION	\"$${git_version}\""; \
		if [ x"$${old_version}" != x"$${new_version}" ]; then \
			sed -i "s'$${version_match}'$${new_version}'" "$${version_h}"; \
		else \
			echo -n " (cached)"; \
		fi; \
		echo -n "... "; \
	fi)
	@echo "ok"

##
## version.h is defined as a .PHONY file even if it's a real file as
## we want our routine to be runned for each build.
##
.PHONY: version.h

nodist_noinst_HEADERS = \
	version.h

lttnginclude_HEADERS = \
	lttng/health.h \
	lttng/lttng.h \
	lttng/constant.h \
	lttng/channel.h \
	lttng/domain.h \
	lttng/event.h \
	lttng/handle.h \
	lttng/session.h \
	lttng/lttng-error.h \
	lttng/snapshot.h \
	lttng/save.h \
	lttng/load.h \
	version.h.tmpl

noinst_HEADERS = \
	lttng/snapshot-internal.h \
	lttng/health-internal.h \
	lttng/save-internal.h \
	lttng/load-internal.h
