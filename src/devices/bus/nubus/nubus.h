// license:BSD-3-Clause
// copyright-holders:R. Belmont
/***************************************************************************

  nubus.h - NuBus bus and card emulation

  by R. Belmont, based on Miodrag Milanovic's ISA8/16 implementation

***************************************************************************/

#ifndef MAME_BUS_NUBUS_NUBUS_H
#define MAME_BUS_NUBUS_NUBUS_H

#pragma once

#include "screen.h"

#include <functional>
#include <utility>
#include <vector>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class nubus_device;

// ======================> device_nubus_card_interface

// class representing interface-specific live nubus card
class device_nubus_card_interface : public device_interface
{
	friend class nubus_device;
public:
	// construction/destruction
	virtual ~device_nubus_card_interface();

	// helper functions for card devices
	void install_declaration_rom(const char *romregion, bool mirror_all_mb = false, bool reverse_rom = false);
	void install_bank(offs_t start, offs_t end, void *data);
	void install_view(offs_t start, offs_t end, memory_view &view);

	u32 get_slotspace() { return 0xf0000000 | (m_slot<<24); }
	u32 get_super_slotspace() { return m_slot<<28; }

	void raise_slot_irq();
	void lower_slot_irq();
	void slot_irq_w(int state);

	void set_pds_slot(int slot) { m_slot = slot; }

	void set_nubus_tag(nubus_device *nubus, const char *slottag) { m_nubus = nubus; m_nubus_slottag = slottag; }

protected:
	device_nubus_card_interface(const machine_config &mconfig, device_t &device);
	virtual void interface_pre_start() override;

	int slotno() const { assert(m_nubus); return m_slot; }
	nubus_device &nubus() { assert(m_nubus); return *m_nubus; }

private:
	nubus_device *m_nubus;
	const char *m_nubus_slottag;
	int m_slot;
	std::vector<u8> m_declaration_rom;
};
class nubus_slot_device : public device_t, public device_single_card_slot_interface<device_nubus_card_interface>
{
public:
	// construction/destruction
	template <typename T, typename U>
	nubus_slot_device(const machine_config &mconfig, T &&tag, device_t *owner, const char *nbtag, U &&opts, const char *dflt)
		: nubus_slot_device(mconfig, tag, owner, (u32)0)
	{
		option_reset();
		opts(*this);
		set_default_option(dflt);
		set_nubus_slot(std::forward<T>(nbtag), tag);
	}

	nubus_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	// inline configuration
	template <typename T>
	void set_nubus_slot(T &&tag, const char *slottag)
	{
		m_nubus.set_tag(std::forward<T>(tag));
		m_nubus_tag = tag;
		m_nubus_slottag = slottag;
	}

	const char *get_nubus_bustag() { return m_nubus_tag; }

protected:
	nubus_slot_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock);

	// device_t implementation
	virtual void device_resolve_objects() override ATTR_COLD;
	virtual void device_start() override ATTR_COLD;

	// configuration
	required_device<nubus_device> m_nubus;
	const char *m_nubus_tag;
	const char *m_nubus_slottag;
};

// device type definition
DECLARE_DEVICE_TYPE(NUBUS_SLOT, nubus_slot_device)


class device_nubus_card_interface;
// ======================> nubus_device
class nubus_device : public device_t, public device_memory_interface
{
public:
	// construction/destruction
	nubus_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);
	~nubus_device();

	// inline configuration
	template <typename T> void set_space(T &&tag, int spacenum) { m_space.set_tag(std::forward<T>(tag), spacenum); }
	auto out_irq9_callback() { return m_out_irq9_cb.bind(); }
	auto out_irqa_callback() { return m_out_irqa_cb.bind(); }
	auto out_irqb_callback() { return m_out_irqb_cb.bind(); }
	auto out_irqc_callback() { return m_out_irqc_cb.bind(); }
	auto out_irqd_callback() { return m_out_irqd_cb.bind(); }
	auto out_irqe_callback() { return m_out_irqe_cb.bind(); }

	typedef enum NUBUS_MODE_T
	{
		NORMAL = 0,
		QUADRA_DAFB,        // omits slot $9 space for DAFB
		LC_PDS,             // takes slot $E space only, with A31 in both states, for V8 based systems
		LC32_PDS,           // takes slots $C, $D, and $E for Sonora-based systems
		SE30                // omits slot $E space for SE/30 internal video
	} nubus_mode_t;
	void set_bus_mode(nubus_mode_t newMode) { m_bus_mode = newMode; }

	void add_nubus_card(device_nubus_card_interface &card);
	template <typename R, typename W> void install_device(offs_t start, offs_t end, R rhandler, W whandler, u32 mask=0xffffffff);
	template <typename R> void install_readonly_device(offs_t start, offs_t end, R rhandler, u32 mask=0xffffffff);
	template <typename W> void install_writeonly_device(offs_t start, offs_t end, W whandler, u32 mask=0xffffffff);
	void install_bank(offs_t start, offs_t end, void *data);
	void install_view(offs_t start, offs_t end, memory_view &view);

	/// \brief Installs a map for the slot's 16 MiB slot space, $Fs00'0000-$FsFF'FFFF
	template <typename T>
	void install_map(T &device, void (T::*map)(address_map &map))
	{
		const offs_t start = device.get_slotspace();
		const offs_t end = (start + 0x01000000) - 1;

		space(AS_DATA).install_device(start, end, device, map);
	}

	/// \brief Installs a map for the slot's 256 MiB super slot space, $s000'0000-$sFFF'FFFF
	template <typename T>
	void install_super_map(T &device, void (T::*map)(address_map &map))
	{
		const offs_t start = device.get_super_slotspace();
		const offs_t end = (start + 0x10000000) - 1;

		space(AS_DATA).install_device(start, end, device, map);
	}

	/// \brief Installs a "free-form" map for LC PDS cards, which need to get outside of the box
	template <typename T>
	void install_lcpds_map(T &device, void (T::*map)(address_map &map))
	{
		space(AS_DATA).install_device(0x0000'0000, 0xffff'ffff, device, map);
	}

	void set_irq_line(int slot, int state);
	void set_address_mask(u32 mask) { m_addr_mask = mask; }

	void irq9_w(int state);
	void irqa_w(int state);
	void irqb_w(int state);
	void irqc_w(int state);
	void irqd_w(int state);
	void irqe_w(int state);

protected:
	nubus_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock);

	// device_t implementation
	virtual void device_start() override ATTR_COLD;

	virtual space_config_vector memory_space_config() const override;

	template <u32 Base> u32 bus_memory_r(offs_t offset, u32 mem_mask);
	template <u32 Base> void bus_memory_w(offs_t offset, u32 data, u32 mem_mask);
	template <u32 Base> u32 host_memory_r(offs_t offset, u32 mem_mask);
	template <u32 Base> void host_memory_w(offs_t offset, u32 data, u32 mem_mask);

	// internal state
	required_address_space m_space;

	address_space_config m_mem_config;

	devcb_write_line    m_out_irq9_cb;
	devcb_write_line    m_out_irqa_cb;
	devcb_write_line    m_out_irqb_cb;
	devcb_write_line    m_out_irqc_cb;
	devcb_write_line    m_out_irqd_cb;
	devcb_write_line    m_out_irqe_cb;

	std::vector<std::reference_wrapper<device_nubus_card_interface> > m_device_list;

	nubus_mode_t m_bus_mode;
	u32 m_addr_mask;
};

inline void device_nubus_card_interface::raise_slot_irq()
{
	nubus().set_irq_line(m_slot, ASSERT_LINE);
}

inline void device_nubus_card_interface::lower_slot_irq()
{
	nubus().set_irq_line(m_slot, CLEAR_LINE);
}

inline void device_nubus_card_interface::slot_irq_w(int state)
{
	if (state)
	{
		nubus().set_irq_line(m_slot, ASSERT_LINE);
	}
	else
	{
		nubus().set_irq_line(m_slot, CLEAR_LINE);
	}
}

// device type definition
DECLARE_DEVICE_TYPE(NUBUS, nubus_device)

DECLARE_DEVICE_TYPE(MACSE30_PDS_BUS, se30_pds_bus_device);
class se30_pds_bus_device: public nubus_device
{
public:
	se30_pds_bus_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
		: nubus_device(mconfig, MACSE30_PDS_BUS, tag, owner, clock),
		m_internal_screen(*this, finder_base::DUMMY_TAG)
	{
		m_bus_mode = SE30;
	}

	template <typename... T>
	void set_screen_tag(T &&...args) { m_internal_screen.set_tag(std::forward<T>(args)...); }

	required_device<screen_device> m_internal_screen;
};

#endif  // MAME_BUS_NUBUS_NUBUS_H
