CC =  gcc
G++ = g++
OPT = -std=gnu++11
GOTPSSO = -shared -fPIC

KERNEL_VERSION := $(shell uname -r)
KERNEL_MAJ := $(shell echo $(KERNEL_VERSION) | cut -f1 -d.)
KERNEL_MIN := $(shell echo $(KERNEL_VERSION) | cut -f2 -d.)

BUILDDIR = ../build
ROOTDIR = ../

CM_TESTS_EXCLUDE = BaseTestCase.cpp
CM_TESTS = \
	$(patsubst %.cpp, %.so, \
		$(filter-out $(CM_TESTS_EXCLUDE), \
			$(notdir $(wildcard tests/*.cpp))))

CM_SEQ1_EXCLUDE = base.cpp
CM_SEQ1 = \
	$(patsubst %.cpp, %.so, \
		$(filter-out $(CM_SEQ1_EXCLUDE), \
			$(notdir $(wildcard $(CURDIR)/tests/seq1/*.cpp))))
CM_SEQ1_OUT = \
	$(foreach TEST, $(CM_SEQ1), \
		$(BUILDDIR)/tests/seq1/$(TEST))

TARGET = $(ROOTDIR)harness

.PHONY: all tests seq1

all: $(BUILDDIR) $(TARGET) tests seq1

$(BUILDDIR): 
	@mkdir -p $(BUILDDIR)

$(TARGET): \
	$(ROOTDIR)/src/main.cpp \
	$(BUILDDIR)/communication/ClientSocket.o \
	$(BUILDDIR)/tests/BaseTestCase.o \
	$(BUILDDIR)/results/DataTestResult.o \
	$(BUILDDIR)/results/FileSystemTestResult.o \
	$(BUILDDIR)/results/SingleTestInfo.o \
	$(BUILDDIR)/results/TestSuiteResult.o \
	$(BUILDDIR)/wrapper/DiskMod.o \
	$(BUILDDIR)/wrapper/wrapper.o \
	$(BUILDDIR)/wrapper/actions.o \
	$(BUILDDIR)/utils/FSCommands.o \
	$(BUILDDIR)/utils/Tester.o \
	$(BUILDDIR)/checker/DiskContents.o
	$(G++) $(OPT) $^ -ldl -o $@

tests: \
	$(foreach TEST, $(CM_TESTS), $(BUILDDIR)/tests/$(TEST))

seq1: $(CM_SEQ1_OUT)

$(BUILDDIR)/communication/%.o: communication/%.cpp
	@mkdir -p $(@D)
	$(G++) -c $(OPT) -fPIC -o $@ $<

$(BUILDDIR)/wrapper/%.o: wrapper/%.cpp
	@mkdir -p $(@D)
	$(G++) -c $(OPT) -fPIC -o $@ $<

$(BUILDDIR)/results/%.o: results/%.cpp
	@mkdir -p $(@D)
	$(G++) -c $(OPT) -fPIC -o $@ $<

$(BUILDDIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(@D)
	$(G++) -c $(OPT) -fPIC -o $@ $<

$(BUILDDIR)/utils/%.o: utils/%.cpp
	@mkdir -p $(@D)
	$(G++) -c $(OPT) -fPIC -o $@ $<

$(BUILDDIR)/checker/%.o: checker/%.cpp
	@mkdir -p $(@D)
	$(G++) -c $(OPT) -fPIC -o $@ $<

$(BUILDDIR)/tests/%.so: tests/%.cpp \
		$(BUILDDIR)/tests/BaseTestCase.o \
		$(BUILDDIR)/wrapper/actions.o \
		$(BUILDDIR)/wrapper/workload.o \
		$(BUILDDIR)/wrapper/wrapper.o \
		$(BUILDDIR)/wrapper/DiskMod.o \
		$(BUILDDIR)/results/DataTestResult.o
	@mkdir -p $(@D)
	$(G++) $(OPT) $(GOTPSSO) $(XATTR_DEF_FLAG) -Wl,-soname,$(notdir $@) \
-o $@ $^

# TODO FIX LINK ERRORS
$(CM_SEQ1_OUT): $(BUILDDIR)/tests/seq1/%.so : \
                tests/seq1/%.cpp \
                $(BUILDDIR)/tests/BaseTestCase.o \
                $(BUILDDIR)/wrapper/actions.o \
		$(BUILDDIR)/wrapper/workload.o \
                $(BUILDDIR)/wrapper/wrapper.o \
                $(BUILDDIR)/wrapper/DiskMod.o \
                $(BUILDDIR)/communication/ClientSocket.o \
                $(BUILDDIR)/results/DataTestResult.o
	@mkdir -p $(@D)
	$(G++) $(OPT) $(GOTPSSO) $(XATTR_DEF_FLAG) -Wl,-soname,$(notdir $@) \
                -o $@ $^

