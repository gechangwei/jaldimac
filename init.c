/*
 * jaldimac
 */
 
#include "jaldi.h"

/* We use the hw_value as an index into our private channel structure */

#define CHAN2G(_freq, _idx)  { \
	.channel = (_freq), \
	.center_freq = (_freq), \
	.hw_value = (_idx), \
	.max_power = 20, \
}

#define CHAN5G(_freq, _idx) { \
	.channel = (_freq), \
	.center_freq = (_freq), \
	.hw_value = (_idx), \
	.max_power = 20, \
}

/* From a9k:
 * "Some 5 GHz radios are actually tunable on XXXX-YYYY
 * on 5 MHz steps, we support the channels which we know
 * we have calibration data for all cards though to make
 * this static"
 *
 * Because we (JaldiMAC) are not doing anything with calibration yet,
 * perhaps we can leverage this capability. TODO: does 9280 support this?
 */
static struct jadli_channel jaldi_5ghz_chantable[] = {
	/* _We_ call this UNII 1 */
	CHAN5G(5180, 14), /* Channel 36 */
	CHAN5G(5200, 15), /* Channel 40 */
	CHAN5G(5220, 16), /* Channel 44 */
	CHAN5G(5240, 17), /* Channel 48 */
	/* _We_ call this UNII 2 */
	CHAN5G(5260, 18), /* Channel 52 */
	CHAN5G(5280, 19), /* Channel 56 */
	CHAN5G(5300, 20), /* Channel 60 */
	CHAN5G(5320, 21), /* Channel 64 */
	/* _We_ call this "Middle band" */
	CHAN5G(5500, 22), /* Channel 100 */
	CHAN5G(5520, 23), /* Channel 104 */
	CHAN5G(5540, 24), /* Channel 108 */
	CHAN5G(5560, 25), /* Channel 112 */
	CHAN5G(5580, 26), /* Channel 116 */
	CHAN5G(5600, 27), /* Channel 120 */
	CHAN5G(5620, 28), /* Channel 124 */
	CHAN5G(5640, 29), /* Channel 128 */
	CHAN5G(5660, 30), /* Channel 132 */
	CHAN5G(5680, 31), /* Channel 136 */
	CHAN5G(5700, 32), /* Channel 140 */
	/* _We_ call this UNII 3 */
	CHAN5G(5745, 33), /* Channel 149 */
	CHAN5G(5765, 34), /* Channel 153 */
	CHAN5G(5785, 35), /* Channel 157 */
	CHAN5G(5805, 36), /* Channel 161 */
	CHAN5G(5825, 37), /* Channel 165 */
};

/* List of rates we can select from. 
 * TODO: What is the hardware limit for these rates? 
 */
#define RATE(_bitrate, _hw_rate) {              \
	.bitrate        = (_bitrate),                   \
	.hw_value       = (_hw_rate),                   \
}

static struct jaldi_rate jaldi_rates[] = {
	RATE(10, 0x1b),
	RATE(20, 0x1a),
	RATE(55, 0x19),
	RATE(110, 0x18),
	RATE(60, 0x0b),
	RATE(90, 0x0f),
	RATE(120, 0x0a),
	RATE(180, 0x0e),
	RATE(240, 0x09),
	RATE(360, 0x0d),
	RATE(480, 0x08),
	RATE(540, 0x0c),
};

static void jaldi_iowrite32(struct jaldi_hw *hw, u32 val, u32 reg_offset) {
	struct jaldi_softc *sc = hw->hw_sc;

	if (hw->serialize_regmode = SER_REG_MODE_ON) {
		unsigned long flags;
		spin_lock_irqsave(&sc->sc_serial_rw, flags);
		iowrite32(val, sc->mem + reg_offset);
		spin_unlock_irqrestore(&sc->sc_serial_rw, flags);
	} else {
		iowrite32(val, sc->mem + reg_offset);
	}
}

static void jaldi_ioread32(struct jaldi_hw *hw, u32 reg_offset) {
	struct jaldi_softc *sc = hw->hw_sc;
	u32 val; 

	if (hw->serialize_regmode = SER_REG_MODE_ON) {
		unsigned long flags;
		spin_lock_irqsave(&sc->sc_serial_rw, flags);
		val = ioread32(sc->mem + reg_offset);
		spin_unlock_irqrestore(&sc->sc_serial_rw, flags);
	} else {
		val = ioread32(sc->mem + reg_offset);
	}
}

static const struct jaldi_register_ops jaldi_reg_ops = {
	.read = jaldi_ioread32,
	.write = jaldi_iowrite32,
};

int jaldi_tx_init(struct jaldi_softc *sc, int nbufs)
{
	int error = 0;
	
	spin_lock_init(&sc->txbufferlock);


}

int jaldi_init_softc(u16 devid, struct jaldi_softc *sc, u16 subsyid, const struct jaldi_bus_ops *bus_ops)
{
	struct jaldi_hw *hw = NULL;
	int ret = 0, i;

	hw = kzalloc(sizeof(struct jaldi_hw), GFP_KERNEL);
	if (!hw) return -ENOMEM;

	hw->hw_version.devid = devid;
	hw->hw_version.subsysid = subsysid;
	sc->hw = hw;

	spin_lock_init(&sc->sc_resetlock);
	spin_lock_init(&sc->sc_netdevlock);
	/* init tasklets and other locks here */

	/* ath9k reads cache line size here... may be relevant */
	ret = jaldi_hw_init(hw);
	if (ret) goto err_hw;

	ret = jaldi_init_queues(sc);
	if (ret) goto err_queues;
	
err_queues:
	jaldi_hw_deinit(hw);
err_hw:
	/* need to kill any tasklets we started */
	kfree(hw);
	sc->hw = NULL;
	jaldi_print(JALDI_FATAL,"init_device failed, ret=%d\n",ret);
	return ret;

}

int jaldi_init_device(u16 devid, struct jaldi_softc *sc, u16 subsyid, const struct jaldi_bus_ops *bus_ops)
{
	jaldi_hw *hw;
	int error;

	error = jaldi_init_softc(devid, sc, subsysid, bus_ops);
	if (error != 0)
		goto error_init;

	hw = sc->hw;

	/* Setup TX DMA */
	error = jaldi_tx_init(sc, JALDI_NUM_TXBUF); // TODO
	if (error) { goto error_tx; }

	/* Setup RX DMA */
	error = jaldi_rx_init(sc, JALDI_NUM_RXBUF); // TODO
	if (error) { goto error_rx; }

	/* initialize workers here if needed */

	return 0;

error_rx:
	jaldi_tx_cleanup(sc);
error_tx:
	jaldi_deinit_softc(sc);
error_init:
	jaldi_print(JALDI_FATAL, "init_device failed, error %d.\n",error);
	return error;
}