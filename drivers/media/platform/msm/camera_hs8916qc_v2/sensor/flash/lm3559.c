/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/export.h>
#include "msm_camera_io_util.h"
#include "msm_led_flash.h"

#define FLASH_NAME "ti,lm3559"
#define CAM_FLASH_PINCTRL_STATE_SLEEP "cam_flash_suspend"
#define CAM_FLASH_PINCTRL_STATE_DEFAULT "cam_flash_default"

#define CONFIG_MSMB_CAMERA_DEBUG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define LM3559_DBG(fmt, args...) pr_err(fmt, ##args)
#else
#define LM3559_DBG(fmt, args...)
#endif

/* Registers */
#define LM3559_ENABLE				0x10
#define LM3559_MI_CONF1				0x12
#define LM3559_MI_CONF2				0x13
#define LM3559_TORCH_BRT			0xA0
#define LM3559_FLASH_BRT			0xB0
#define LM3559_FLASH_CONFIG			0xC0
#define LM3559_FLAGS				0xD0
#define LM3559_CONFIG1				0xE0
#define LM3559_LAST_FLASH			0x81
#define LM3559_GPIO_CONF        	0x20

/* Mask/Shift/Value */
#define LM3559_FLASH_M				0x3F	/* ENABLE register*/
#define LM3559_FLASH_ON				0x1B
#define LM3559_FLASH_OFF			0x00
#define LM3559_DEVICE_OFF			0x00
#define LM3559_MI_M					0xC0
#define LM3559_MI_ON				0xC0
#define LM3559_FLASH_TIMEOUT_M		0x1F	/* FLASH_CONFIG register */
#define LM3559_MI_MODE_M			0x10	/* CONFIG1 register */
#define LM3559_MI_MODE_OUT			0x00
#define LM3559_TORCH_ON         	0x1A
#define LM3559_FLASH_TIMEOUT_FLG    0x01
#define LM3559_HARDWARE_TORCH		0x80

#define MAX_FLASH_BRIGHTNESS		16
#define LM3559_MAX_TIMEOUT			31

static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver lm3559_i2c_driver;

static struct msm_camera_i2c_reg_array lm3559_init_array[] = {
	//{ LM3559_CONFIG1,   0x6c },
	{ LM3559_FLASH_BRT, 0x00},
	{ LM3559_ENABLE,    0x18},
    { LM3559_FLASH_CONFIG, 0x0f}, 


};

static struct msm_camera_i2c_reg_array lm3559_off_array[] = {
	{LM3559_ENABLE, 0x00},
};

static struct msm_camera_i2c_reg_array lm3559_led1_low_array[] = {
	{LM3559_TORCH_BRT, 0x09},
	{LM3559_ENABLE, 0x0a},
};

static struct msm_camera_i2c_reg_array lm3559_led2_low_array[] = {
	{LM3559_TORCH_BRT, 0x09},
	{LM3559_ENABLE, 0x12},
};

static struct msm_camera_i2c_reg_array lm3559_led1_high_array[] = {
	{LM3559_FLASH_BRT, 0xdd},
	{LM3559_ENABLE, 0x0b},
};

static struct msm_camera_i2c_reg_array lm3559_led2_high_array[] = {
	{LM3559_FLASH_BRT, 0xdd},
	{LM3559_ENABLE, 0x13},
};




static struct msm_camera_i2c_reg_array lm3559_release_array[] = {
	{LM3559_ENABLE, 0x00},
};

static struct msm_camera_i2c_reg_array lm3559_low_array[] = {
	{LM3559_TORCH_BRT, 0x1b},  //qxd make two led torch together,225ma
	{LM3559_ENABLE, 0x1a},   

	
};

static struct msm_camera_i2c_reg_array lm3559_high_array[] = {
	{LM3559_FLASH_BRT, 0xdd},  //qxd make two led flash together,1575ma
	{LM3559_ENABLE, 0x1b}, 

		
};

static void __exit msm_flash_lm3559_i2c_remove(void)
{
	i2c_del_driver(&lm3559_i2c_driver);
	return;
}

static const struct of_device_id lm3559_i2c_trigger_dt_match[] = {
	{.compatible = "ti,lm3559", .data = &fctrl},
	{}
};

MODULE_DEVICE_TABLE(of, lm3559_i2c_trigger_dt_match);

static const struct i2c_device_id flash_i2c_id[] = {
	{"ti,lm3559", (kernel_ulong_t)&fctrl},
	{ }
};

static const struct i2c_device_id lm3559_i2c_id[] = {
	{FLASH_NAME, (kernel_ulong_t)&fctrl},
	{ }
};
static int msm_flash_pinctrl_init(struct msm_led_flash_ctrl_t *ctrl)
{
	struct msm_pinctrl_info *flash_pctrl = NULL;
	flash_pctrl = &ctrl->pinctrl_info;
	if (flash_pctrl->use_pinctrl != true) {
		pr_err("%s: %d PINCTRL is not enables in Flash driver node\n",
			__func__, __LINE__);
		return 0;
	}
	flash_pctrl->pinctrl = devm_pinctrl_get(&ctrl->pdev->dev);

	if (IS_ERR_OR_NULL(flash_pctrl->pinctrl)) {
		pr_err("%s:%d Getting pinctrl handle failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	flash_pctrl->gpio_state_active = pinctrl_lookup_state(
					       flash_pctrl->pinctrl,
					       CAM_FLASH_PINCTRL_STATE_DEFAULT);

	if (IS_ERR_OR_NULL(flash_pctrl->gpio_state_active)) {
		pr_err("%s:%d Failed to get the active state pinctrl handle\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	flash_pctrl->gpio_state_suspend = pinctrl_lookup_state(
						flash_pctrl->pinctrl,
						CAM_FLASH_PINCTRL_STATE_SLEEP);

	if (IS_ERR_OR_NULL(flash_pctrl->gpio_state_suspend)) {
		pr_err("%s:%d Failed to get the suspend state pinctrl handle\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

int msm_flash_lm3559_led_init(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	LM3559_DBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;

	msm_flash_pinctrl_init(fctrl);

	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	if (fctrl->pinctrl_info.use_pinctrl == true) {
		pr_err("%s:%d PC:: flash pins setting to active state",
				__func__, __LINE__);
		rc = pinctrl_select_state(fctrl->pinctrl_info.pinctrl,
				fctrl->pinctrl_info.gpio_state_active);
		if (rc)
			pr_err("%s:%d cannot set pin to active state",
					__func__, __LINE__);
	}

	msleep(20);
	//flash touch gpio both operate ,truly,register seting is working
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_HIGH);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->init_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	return rc;
}

int msm_flash_lm3559_led_release(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	LM3559_DBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);

	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 0);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	if (fctrl->pinctrl_info.use_pinctrl == true) {
		rc = pinctrl_select_state(fctrl->pinctrl_info.pinctrl,
				fctrl->pinctrl_info.gpio_state_suspend);
		if (rc)
			pr_err("%s:%d cannot set pin to suspend state",
				__func__, __LINE__);
	}
	return 0;
}

int msm_flash_lm3559_led_off(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	LM3559_DBG("%s:%d called\n", __func__, __LINE__);

	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->off_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);

	return rc;
}

int msm_flash_lm3559_led_low(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	LM3559_DBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;


	gpio_set_value_cansleep(
			power_info->gpio_conf->gpio_num_info->
			gpio_num[SENSOR_GPIO_FL_EN],
			GPIO_OUT_HIGH);
	
		gpio_set_value_cansleep(
			power_info->gpio_conf->gpio_num_info->
			gpio_num[SENSOR_GPIO_FL_NOW],
			GPIO_OUT_HIGH);


	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->low_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}

int msm_flash_lm3559_led_high(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	LM3559_DBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;

	power_info = &flashdata->power_info;

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_HIGH);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->high_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}

static int msm_flash_lm3559_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	LM3559_DBG("%s entry\n", __func__);
	if (!id) {
		pr_err("msm_flash_lm3559_i2c_probe: id is NULL");
		id = lm3559_i2c_id;
	}

	return msm_flash_i2c_probe(client, id);
}

static struct i2c_driver lm3559_i2c_driver = {
	.id_table = lm3559_i2c_id,
	.probe  = msm_flash_lm3559_i2c_probe,
	.remove = __exit_p(msm_flash_lm3559_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lm3559_i2c_trigger_dt_match,
	},
};

static int msm_flash_lm3559_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_device(lm3559_i2c_trigger_dt_match, &pdev->dev);
	if (!match)
		return -EFAULT;
	return msm_flash_probe(pdev, match->data);
}

static struct platform_driver lm3559_platform_driver = {
	.probe = msm_flash_lm3559_platform_probe,
	.driver = {
		.name = "lm3559",
		.owner = THIS_MODULE,
		.of_match_table = lm3559_i2c_trigger_dt_match,
	},
};
static int __init msm_flash_lm3559_init_module(void)
{
	int32_t rc = 0;
	LM3559_DBG("%s: enter \n", __func__);

	rc = platform_driver_register(&lm3559_platform_driver);
	if (!rc)
	{
		LM3559_DBG("%s:%d rc %d\n", __func__, __LINE__, rc);	
		return rc;
	}
	LM3559_DBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&lm3559_i2c_driver);
}
static void __exit msm_flash_lm3559_exit_module(void)
{
	if (fctrl.pdev)
		platform_driver_unregister(&lm3559_platform_driver);
	else
		i2c_del_driver(&lm3559_i2c_driver);
}
static struct msm_camera_i2c_client lm3559_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_i2c_reg_setting lm3559_init_setting = {
	.reg_setting = lm3559_init_array,
	.size = ARRAY_SIZE(lm3559_init_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3559_led1_low_setting = {
	.reg_setting = lm3559_led1_low_array,
	.size = ARRAY_SIZE(lm3559_led1_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3559_led2_low_setting = {
	.reg_setting = lm3559_led2_low_array,
	.size = ARRAY_SIZE(lm3559_led2_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3559_led1_high_setting = {
	.reg_setting = lm3559_led1_high_array,
	.size = ARRAY_SIZE(lm3559_led1_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3559_led2_high_setting = {
	.reg_setting = lm3559_led2_high_array,
	.size = ARRAY_SIZE(lm3559_led2_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};




static struct msm_camera_i2c_reg_setting lm3559_off_setting = {
	.reg_setting = lm3559_off_array,
	.size = ARRAY_SIZE(lm3559_off_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3559_release_setting = {
	.reg_setting = lm3559_release_array,
	.size = ARRAY_SIZE(lm3559_release_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3559_low_setting = {
	.reg_setting = lm3559_low_array,
	.size = ARRAY_SIZE(lm3559_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3559_high_setting = {
	.reg_setting = lm3559_high_array,
	.size = ARRAY_SIZE(lm3559_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_led_flash_reg_t lm3559_regs = {
	.init_setting = &lm3559_init_setting,
	.off_setting = &lm3559_off_setting,
	.low_setting = &lm3559_low_setting,
	.high_setting = &lm3559_high_setting,
	.release_setting = &lm3559_release_setting,
	.led1_low_setting = &lm3559_led1_low_setting,
	.led2_low_setting = &lm3559_led2_low_setting,
	.led1_high_setting = &lm3559_led1_high_setting,
	.led2_high_setting = &lm3559_led2_high_setting,
	
};

static struct msm_flash_fn_t lm3559_func_tbl = {
	.flash_get_subdev_id = msm_led_i2c_trigger_get_subdev_id,//use platform's interface 
	.flash_led_config = msm_led_i2c_trigger_config,
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &lm3559_i2c_client,
	.reg_setting = &lm3559_regs,
	.func_tbl = &lm3559_func_tbl,
	.flag_error_addr = LM3559_FLAGS,
};

module_init(msm_flash_lm3559_init_module);
module_exit(msm_flash_lm3559_i2c_remove);
MODULE_DESCRIPTION("lm3559 FLASH");
MODULE_LICENSE("GPL v2");
