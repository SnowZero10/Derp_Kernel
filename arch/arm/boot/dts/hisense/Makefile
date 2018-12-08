ifeq ($(CONFIG_OF),y)

ifneq ($(subst ",,$(CONFIG_BOARD_INFO)),)
    ifneq ($(subst ",,$(CONFIG_HARDWARE_VERSION_INFO)),)
        ifneq ($(subst ",,$(CONFIG_PRODUCT_INFO)),)
            dtb-y += $(subst ",,$(CONFIG_BOARD_INFO))-$(subst ",,$(CONFIG_HARDWARE_VERSION_INFO))-$(subst ",,$(CONFIG_PRODUCT_INFO)).dtb
        endif
    endif
endif


DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(addprefix ../, $(DTB_LIST))
endif

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(DTB_LIST))

clean-files := *.dtb
