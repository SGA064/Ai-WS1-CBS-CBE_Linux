/* Reinitialize a WS73 left powered after a firmware-side reset, Linux 6.18+. */
#ifndef HCC_SDIO_RELOAD_H
#define HCC_SDIO_RELOAD_H

#ifdef __KERNEL__
#include <linux/device.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#endif

static bool ws73_sdio_reload_target(struct sdio_func *func)
{
    return func && func->card && func->num == 1 &&
        mmc_card_sdio(func->card) && func->card->sdio_funcs == 1 &&
        ((func->vendor == 0xffff && func->device == 0x3733) ||
         (func->vendor == 0x12d1 && func->device == 0x0073));
}

/* The caller holds a device reference. Refuse active drivers, IRQs, shared
 * cards and SD storage; serialize with probe/remove and the MMC host. */
static int ws73_sdio_reset_unbound(struct sdio_func *func)
{
    struct mmc_card *card;
    struct mmc_host *host;
    int ret = -ENODEV;

    if (!ws73_sdio_reload_target(func))
        return ret;
    card = func->card;
    host = card->host;
    device_lock(&func->dev);
    if (func->dev.driver || func->irq_handler) {
        ret = -EBUSY;
        goto unlock;
    }
    /* Hosts with card runtime PM already restore power and enumeration
     * before probe. This recovery is for the always-powered ePass slot. */
    if (host->caps & MMC_CAP_POWER_OFF_CARD) {
        ret = -EOPNOTSUPP;
        goto unlock;
    }
    sdio_claim_host(func);
    if (host->card == card && host->bus_ops &&
        device_is_registered(&func->dev) && device_is_registered(&card->dev) &&
        (!host->ops->get_cd || host->ops->get_cd(host) > 0)) {
        /* Reset at the initial clock, then restore RCA, bus width and
         * high-speed timing through the MMC core, without cycling rails. */
        ret = mmc_sw_reset(card);
    }
    sdio_release_host(func);
unlock:
    device_unlock(&func->dev);
    return ret;
}

static int ws73_sdio_unbound_match(struct device *dev, const void *unused)
{
    (void)unused;
    return !dev->driver && ws73_sdio_reload_target(dev_to_sdio_func(dev));
}

/* Retry one known, unbound card once. Never unbind somebody else's driver. */
static int ws73_sdio_retry_probe(struct sdio_driver *driver)
{
    struct device *dev;
    int ret;

    dev = bus_find_device(driver->drv.bus, NULL, NULL, ws73_sdio_unbound_match);
    if (!dev)
        return -ENODEV;
    ret = ws73_sdio_reset_unbound(dev_to_sdio_func(dev));
    if (!ret)
        ret = device_driver_attach(&driver->drv, dev);
    put_device(dev);
    return ret;
}

#endif
