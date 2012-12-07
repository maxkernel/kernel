MODULES		= console discovery file netui httpserver service
DEFINES		= -D_GNU_SOURCE -DMODULE -D$(RELEASE) -DRELEASE="\"$(RELEASE)\"" -DINSTALL="\"$(INSTALL)\""

all:
	$(foreach module,$(MODULES), python makefile.gen.py --module $(module) --defines '$(DEFINES)' >$(module)/Makefile && $(MAKE) -C $(module) depend &&) true
	$(foreach module,$(MODULES), echo "In module $(module)" >&2 && $(MAKE) -C $(module) all &&) true

install:
	[ -n "$(INSTALL)" ] || ( echo "INSTALL variable must be set" >&2 && false )
	$(foreach module,$(MODULES), $(MAKE) -C $(module) install && ( [ -e $(module)/install.part.bash ] && ( cd $(module) && bash install.part.bash ) || true ) &&) true

clean:
	( $(foreach module,$(MODULES), $(MAKE) -C $(module) clean; ( cd $(module) && bash clean.part.bash );) ) || true
	rm -f $(foreach module,$(MODULES), $(module)/Makefile)

