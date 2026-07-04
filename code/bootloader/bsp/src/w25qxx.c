#include "w25qxx.h"

struct W25QXX_CTRL
{
    const SPI_API_t *spix_api;
    GPIO_Pin_t io_cs;

    void (*set_cs)(W25QXX_CTRL_t *, uint8_t PinState);
    void (*reset)(W25QXX_CTRL_t *);
    uint8_t (*busy_status)(W25QXX_CTRL_t *);
    void (*write_enable)(W25QXX_CTRL_t *);
};

static void w25qxx_init(W25QXX_CTRL_t *ctrl)
{
    ctrl->reset(ctrl);
}
static void w25qxx_set_cs(W25QXX_CTRL_t *ctrl, GPIO_PinState PinState)
{
    // HAL_GPIO_WritePin(ctrl->io_cs.port, ctrl->io_cs.pin, PinState);
    if (PinState != GPIO_PIN_RESET)
        ctrl->io_cs.port->BSRR = ctrl->io_cs.pin;
    else
        ctrl->io_cs.port->BSRR = (uint32_t)ctrl->io_cs.pin << 16U;
}

static void w25qxx_reset(W25QXX_CTRL_t *ctrl)
{
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    uint8_t cmd[] = {RESET_ENABLE_CMD, RESET_MEMORY_CMD};
    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->write(cmd, 2);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_SET);
}

// ¶ÁÈ¡×´Ì¬¼Ä´æÆ÷£¬ÅÐ¶ÏÊÇ·ñÃ¦×´Ì¬
static uint8_t w25qxx_busy_status(W25QXX_CTRL_t *ctrl)
{
    uint8_t cmd = READ_STATUS_REG1_CMD;
    uint8_t status = 0;
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->write(&cmd, 1);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->spix_api->read(&status, 1);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_SET);

    if (status & W25QXX_FSR_BUSY)
        return 1;
    else
        return 0;
}

static uint16_t w25qxx_read_id(W25QXX_CTRL_t *ctrl)
{
    // Èý¸ö0x00Îªdummy×Ö½Ú£¬²»²ÎÓëÍ¨ÐÅ
    uint8_t cmd[] = {0x90, 0x00, 0x00, 0x00};
    uint16_t id;

    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->write(cmd, 4);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->spix_api->read((uint8_t *)&id, 2);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_SET);

    return id;
}

static void w25qxx_write_enable(W25QXX_CTRL_t *ctrl)
{
    uint8_t cmd = WRITE_ENABLE_CMD;

    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->write(&cmd, 1);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_SET);
}

static void w25qxx_write(W25QXX_CTRL_t *ctrl, uint32_t addr, const uint8_t *tx_data, uint32_t size)
{
    if (tx_data == NULL || size == 0)
        return;

    uint16_t w_size = 256 - (addr & 0xff); // Ò»Ò³256×Ö½Ú
    if (w_size >= size)
        w_size = size;

    uint8_t cmd[4] = {
        PAGE_PROG_CMD,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)addr,
    };

    while (w_size > 0)
    {
        while (ctrl->busy_status(ctrl))
            ;
        ctrl->write_enable(ctrl);

        ctrl->set_cs(ctrl, GPIO_PIN_RESET);
        ctrl->spix_api->write(cmd, 4);
        while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
            ;
        ctrl->spix_api->write(tx_data, w_size);
        while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
            ;
        ctrl->set_cs(ctrl, GPIO_PIN_SET);

        tx_data += w_size;
        addr += w_size;
        size -= w_size;
        cmd[1] = (uint8_t)(addr >> 16);
        cmd[2] = (uint8_t)(addr >> 8);
        cmd[3] = (uint8_t)addr;

        w_size = size >= 256 ? 256 : size; // ¿çÒ³Ð´Èë
    }
}

static void w25qxx_read(W25QXX_CTRL_t *ctrl, uint32_t addr, uint8_t *rx_data, uint32_t size)
{
    if (rx_data == NULL || size == 0)
        return;

    uint8_t cmd[] = {
        READ_CMD,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)addr,
    };

    while (ctrl->busy_status(ctrl))
        ;

    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->write(cmd, 4);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->spix_api->read(rx_data, size);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_SET);
}

// ÉÈÇø²Á³ý
static void w25qxx_erase_sector(W25QXX_CTRL_t *ctrl, uint32_t addr)
{
    addr &= ~0x00000fff; // ÉÈÇøµØÖ·±ØÐëÊÇ4KB¶ÔÆëµÄ
    // printf("erase sector addr=%x\n", addr);

    uint8_t cmd[] = {
        SECTOR_ERASE_CMD,
        (uint8_t)(addr >> 16),
        (uint8_t)(addr >> 8),
        (uint8_t)addr,
    };

    while (ctrl->busy_status(ctrl))
        ;
    ctrl->write_enable(ctrl);

    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->write(cmd, 4);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_SET);
}

static void w25qxx_erase_chip(W25QXX_CTRL_t *ctrl)
{
    uint8_t cmd = CHIP_ERASE_CMD;

    while (ctrl->busy_status(ctrl))
        ;
    ctrl->write_enable(ctrl);

    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->write(&cmd, 1);
    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;
    ctrl->set_cs(ctrl, GPIO_PIN_SET);
}

////////////////////////////////////////////
static W25QXX_CTRL_t w25qxx_ctrl = {
    .spix_api = &spi1_api,
    .io_cs = {
        .port = GPIOA,
        .pin = GPIO_PIN_15,
    },

    .set_cs = w25qxx_set_cs,
    .reset = w25qxx_reset,
    .busy_status = w25qxx_busy_status,
    .write_enable = w25qxx_write_enable,
};

static W25QXX_API_t w25qxx_api = {
    .init = w25qxx_init,
    .read_id = w25qxx_read_id,
    .write = w25qxx_write,
    .read = w25qxx_read,
    .erase_sector = w25qxx_erase_sector,
    .erase_chip = w25qxx_erase_chip,
};

const W25QXX_t w25qxx = {
    .ctrl = &w25qxx_ctrl,
    .api = &w25qxx_api,
};
