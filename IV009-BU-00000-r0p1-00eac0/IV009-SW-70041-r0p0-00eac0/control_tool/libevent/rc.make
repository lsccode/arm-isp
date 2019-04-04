override GCCAPP = $(notdir $(GCC))
override GCCDIR = $(dir $(GCC))
override WINDRES = $(GCCDIR)$(subst -gcc,-windres,$(GCCAPP))
