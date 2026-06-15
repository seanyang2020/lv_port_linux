# xosfb Makefile - builds libxosfb.a (self-contained static library)
# Includes all platform dependencies (libmpi.a, sample_comm)

include ../../Makefile.param

MPI_LIBS := $(REL_LIB)/libmpi.a
TARGET := libxosfb.a

XOSFB_SRC := xosfb.c
XOSFB_OBJ := $(XOSFB_SRC:%.c=%.o)

# 公共目标文件
COMM_OBJ := ../../common/sample_comm_sys.o \
            ../../common/sample_comm_vdec.o \
            ../../common/sample_comm_vo.o \
            ../../common/param_config.o

# Additional include for fb_drv_ioc.h
CFLAGS += -I$(MPP_DIR)/drv_include

$(info ========================================)
$(info xosfb build)
$(info MPP_DIR = $(MPP_DIR))
$(info CFLAGS  = $(CFLAGS))
$(info MPI_LIBS = $(MPI_LIBS))
$(info COMM_OBJ = $(COMM_OBJ))
$(info ========================================)

.PHONY: all clean

all: $(TARGET)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

# 合并库：单行shell，统一路径、解压、打包、计数
$(TARGET): $(XOSFB_OBJ)
	@echo "Creating combined $@ ..."
	@rm -rf .combine $(TARGET)
	@mkdir -p .combine
	@( \
	cd .combine; \
	$(AR) rcs tmp1.a $(addprefix ../,$(XOSFB_OBJ) $(COMM_OBJ)); \
	$(AR) x tmp1.a; \
	$(AR) x $(MPI_LIBS); \
	$(AR) rcs ../$(TARGET) *.o; \
	CNT=`$(AR) t ../$(TARGET) | wc -l`; \
	echo "$(TARGET) built successfully ($$CNT objects)"; \
	)
	@rm -rf .combine

clean:
	@rm -f $(XOSFB_OBJ)
	@rm -f $(TARGET)
	@rm -rf .combine
