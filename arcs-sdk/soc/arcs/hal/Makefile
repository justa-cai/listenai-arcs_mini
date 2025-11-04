TOPDIR=.

# default value
IC_BOARD=1
CHIP=arcs
INNER=0

MKDEFS=CHIP=${CHIP} TGT=${TGT} IC_BOARD=${IC_BOARD} INNER=${INNER} 

.PHONY: all clean allclean

all:
	@if [ 'a$(TGT)' = 'a' ]; then echo TGT not defined, nothind done! && exit 1; fi
	@if [ -f "src/${TGT}/Makefile" ]; then make -C src/${TGT}/ apps ${MKDEFS} ||exit 1; fi
	@if [ -f "src/${CHIP}/${TGT}/Makefile" ]; then make -C src/${CHIP}/${TGT}/ apps ${MKDEFS}  ||exit 1; fi
	@if [ -f "src/bt/${TGT}/Makefile" ]; then make -C src/bt/${TGT}/ apps ${MKDEFS} ||exit 1; fi
	@if [ -f "src/drv_demo/${TGT}/Makefile" ]; then make -C src/drv_demo/${TGT}/ apps ${MKDEFS} ||exit 1; fi

clean:
	@if [ 'a$(TGT)' = 'a' ]; then echo TGT not defined, nothind done! && exit 1; fi
	@if [ -f "src/${TGT}/Makefile" ]; then make -C src/${TGT}/ clean ${MKDEFS} ||exit 1; fi
	@if [ -f "src/${CHIP}/${TGT}/Makefile" ]; then make -C src/${CHIP}/${TGT}/ clean ${MKDEFS}  ||exit 1; fi
	@if [ -f "src/bt/${TGT}/Makefile" ]; then make -C src/bt/${TGT}/ clean ${MKDEFS} ||exit 1; fi
	@if [ -f "src/drv_demo/${TGT}/Makefile" ]; then make -C src/drv_demo/${TGT}/ clean ${MKDEFS} ||exit 1; fi

allclean: clean
	@if [ -f "src/Makefile" ]; then make -C src clean||exit 1; fi
	@if [ -f "modules/Makefile" ]; then make -C modules clean||exit 1; fi
	@if [ -f "chip/Makefile" ]; then make -C chip clean||exit 1; fi
	@if [ -f "bt/Makefile" ]; then make -C bt clean ${MKDEFS} APP_PATH=${CHIP}/rom ||exit 1; fi
	rm -rf lib/*
	rm -rf out/*
