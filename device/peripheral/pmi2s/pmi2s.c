/* ------------------------------------------
 * Copyright (c) 2017, Synopsys, Inc. All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * 1) Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.

 * 3) Neither the name of the Synopsys, Inc., nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * \version 2017.12
 * \date 2017-11-30
 * \author Shuai Wang(Shuai.Wang1@synopsys.com)
--------------------------------------------- */

/**
 * \defgroup	BOARD_PERIPHER_DRV_PMODI2S	Peripheral Driver Pmod I2S CS4344
 * \ingroup	BOARD_PERIPHER_DRIVER
 * \brief	Pmod I2S CS4344 peripheral driver
 * \details
 *		Implement driver for Pmod I2S CS4344 using DesignWare I2S driver.
 */

/**
 * \file
 * \ingroup	BOARD_PERIPHER_DRV_PMODI2S
 * \brief	Pmod I2S CS4344 peripheral driver
 */

/**
 * \addtogroup	BOARD_PERIPHER_DRV_PMODI2S
 * @{
 */
#include "pmi2s.h"
#include "embARC_debug.h"


#define BOARD_I2S_SEND_IIS_ID			DW_I2S_0_ID
#define BOARD_I2S_RECEIVE_IIS_ID		DW_I2S_1_ID

#define I2S_0_EMP_C0 					0x0080000
#define I2S_1_SD_C0 					0x00d0000

static DEV_I2S *dev_i2s_tx_p = NULL;

/**
 * \brief   The transmiter mode's callback function of interrupt mode.
 * \detail  This function will be callback when the whole data has been send and you have to \
 			resatrt the interrupt and point the data buffer in this function.
*/
void pmi2s_tx_isr_restart(DEV_BUFFER *tx_buffer)
{
	dev_i2s_tx_p->i2s_control(I2S_CMD_SET_TXINT_BUF,tx_buffer);
	/* Enable interrupt */
	dev_i2s_tx_p->i2s_control(I2S_CMD_SET_TXINT,(void *)1);
}

/**
 * \brief Error callback function.
 */
void pmi2s_err_isr(void *ptr)
{
	(void)ptr;
	printf("Inter the isr in err!\n");
}

/**
 * \brief Init i2s.
 * \param dev_i2s_tx_p.Pointer pointing to the I2S device.
 * \param pmi2s_init_str.Structure used to init i2s
 * \param dev.Used to init some parameters related with interrupt,and can be NULL when you do not use interrupt mode.
 * \param i2s_tx_isr.Function pointer pointing to he callback function when all data has been sent.
*/
uint32_t pmi2s_init_func(const PMI2S_INIT_STR *pmi2s_init_str,DEV_BUFFER *dev,void (*i2s_isr)(void))
{
	uint32_t div_number;

	DW_I2S_CONFIG *i2s_config_ptr = (DW_I2S_CONFIG *)(dev_i2s_tx_p->i2s_info.i2s_config);
	if (dev_i2s_tx_p == NULL)
	{
		printf("The i2s init func error!\n");
		return -1;
	}
	dev_i2s_tx_p->i2s_info.device = pmi2s_init_str->ope_device;
	dev_i2s_tx_p->i2s_info.mode = pmi2s_init_str->mode;
	i2s_config_ptr->ws_length = pmi2s_init_str->num_sclk;
	i2s_config_ptr->data_res[0] = pmi2s_init_str->data_format;
	i2s_config_ptr->sample_rate[0] = pmi2s_init_str->sample_rate;
	div_number = (MCLK_FREQUENCY_KHZ)/((pmi2s_init_str->sample_rate)*(pmi2s_init_str->num_sclk*16)*2);

	i2s_tx_clk_div(div_number);
	/* flush the tx fifo */
	pmi2s_tx_flush_fifo();
	dev_i2s_tx_p->i2s_open(DEV_MASTER_MODE, I2S_DEVICE_TRANSMITTER);

	/* Enable device */
	dev_i2s_tx_p->i2s_control(I2S_CMD_ENA_DEV,(void *)0);
	/* Enable master clock */
	dev_i2s_tx_p->i2s_control(I2S_CMD_MST_SET_CLK,(void *)1);
	if (pmi2s_init_str->pll_isr_sel == PMI2S_MODE_ISR)
	{
		/* interrupt related */
		dev_i2s_tx_p->i2s_control(I2S_CMD_SET_TXCHET_BUF,(void *)I2S_0_EMP_C0);
		dev_i2s_tx_p->i2s_control(I2S_CMD_SET_TXCB,i2s_isr);
		dev_i2s_tx_p->i2s_control(I2S_CMD_SET_ERRCB,pmi2s_err_isr);
		dev_i2s_tx_p->i2s_control(I2S_CMD_SET_TXINT_BUF,dev);
		/* Enable interrupt */
		dev_i2s_tx_p->i2s_control(I2S_CMD_SET_TXINT,(void *)1);
	}
	return E_OK;
}

/**
 * \brief	Init I2S with the structure of PMI2S_INIT_STR
 * \param	buffer.The point which is pointed to the buffer used in interrupt mode. \
 			This parameter can be NULL when you use polling mode.
*/
int16_t pmi2s_tx_init(uint32_t freq,uint32_t dfmt,uint32_t mode_sel,DEV_BUFFER *buffer,void (*i2s_isr)(void))
{
	PMI2S_INIT_STR pmi2s_init_str;

	pmi2s_init_str.sample_rate = freq;
	pmi2s_init_str.data_format = dfmt;
	pmi2s_init_str.ope_device = I2S_DEVICE_TRANSMITTER;
	pmi2s_init_str.mode = DEV_MASTER_MODE;
	pmi2s_init_str.num_sclk = DW_I2S_WSS_32_CLK;
	pmi2s_init_str.pll_isr_sel = mode_sel;

	dev_i2s_tx_p = i2s_get_dev(BOARD_I2S_SEND_IIS_ID);
	pmi2s_init_func(&pmi2s_init_str,buffer,i2s_isr);

	return 0;
}

/**
 * \brief Write data by i2s.
 * \param dev_i2s_tx_p.Pointer pointing to the I2S device.
 * \param data.Pointer pointed to the data ready to be sent.
 * \param len.The length of data to be sent.
 * \param channel.0.
*/
uint32_t pmi2s_write_data(const void *data,uint32_t len,uint32_t channel)
{
	return dw_i2s_write(dev_i2s_tx_p, data, len, channel);
}

/**
 * \brief Read data by i2s.
 * \param dev_i2s_tx_p.Pointer pointing to the I2S device.
 * \param data.Pointer pointed to the buffer used to receive data.
 * \param len.The length of data to receive.
 * \param channel.0.
*/
uint32_t pmi2s_read_data(void *data,uint32_t len,uint32_t channel)
{
	return dw_i2s_read(dev_i2s_tx_p, data, len, channel);
}

/**
 * \brief Flush transmiter fifo
 * \param dev_i2s_tx_p.Pointer pointing to the I2S device.
 */
void pmi2s_tx_flush_fifo(void)
{
	dev_i2s_tx_p->i2s_control(I2S_CMD_FLUSH_TX,(void *)NULL);
}

/**
 * \brief Flush receive fifo
 * \param dev_i2s_tx_p.Pointer pointing to the I2S device.
 */
void pmi2s_rx_flush_fifo(void)
{
	dev_i2s_tx_p->i2s_control(I2S_CMD_FLUSH_RX,(void *)NULL);
}
/** @} end of group BOARD_PERIPHER_DRV_PMODI2S */