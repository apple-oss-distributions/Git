include Makefile

print-programs:
# OTHER_PROGRAMS contains `git` and `scalar`
# PROGRAMS contains other `git-*` built-ins
# See `src/git/Makefile` for further details
	@echo $(OTHER_PROGRAMS) $(PROGRAMS)

print-vars:
	@echo uname_M=$(uname_M)
	@echo uname_P=$(uname_P)
