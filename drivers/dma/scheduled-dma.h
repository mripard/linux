/*
 * Copyright (C) 2015 Maxime Ripard
 * Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "virt-dma.h"

#ifndef _SCHEDULED_DMA_H_
#define _SCHEDULED_DMA_H_

enum sdma_transfer_type {
	SDMA_TRANSFER_MEMCPY,
	SDMA_TRANSFER_SLAVE,
};

enum sdma_report_status {
	SDMA_REPORT_CHUNK,
	SDMA_REPORT_TRANSFER,
};

struct sdma_desc {
	struct virt_dma_desc	vdesc;

	/* Entry point to our LLI */
	dma_addr_t		p_lli;
	void			*v_lli;
};

struct sdma_channel {
	struct sdma_desc	*desc;
	unsigned int		index;
	struct list_head	node;
	void			*private;
};

struct sdma_request {
	struct dma_slave_config	cfg;
	struct list_head	node;
	struct virt_dma_chan	vchan;

	struct sdma_channel	*chan;
	void			*private;
};

struct sdma_ops {
	/* LLI management operations */
	bool			(*lli_has_next)(void *v_lli);
	void			*(*lli_next)(void *v_lli);
	int			(*lli_init)(void *v_lli, void *sreq_priv,
					    enum sdma_transfer_type type,
					    enum dma_transfer_direction dir,
					    dma_addr_t src,
					    dma_addr_t dst, u32 len,
					    struct dma_slave_config *config);
	void			*(*lli_queue)(void *prev_v_lli, void *v_lli, dma_addr_t p_lli);
	size_t			(*lli_size)(void *v_lli);

	/* Scheduler helper */
	struct sdma_request	*(*validate_request)(struct sdma_channel *chan,
						     struct sdma_request *req);

	/* Transfer Management Functions */
	int			(*channel_pause)(struct sdma_channel *chan);
	int			(*channel_resume)(struct sdma_channel *chan);
	int			(*channel_start)(struct sdma_channel *chan, struct sdma_desc *sdesc);
	int			(*channel_terminate)(struct sdma_channel *chan);
	size_t			(*channel_residue)(struct sdma_channel *chan);
};

struct sdma {
	struct dma_device	ddev;
	struct sdma_ops		*ops;

	struct dma_pool		*pool;

	struct sdma_channel	*channels;
	int			channels_nr;
	struct sdma_request	*requests;
	int			requests_nr;

	struct list_head	avail_chans;
	struct list_head	pend_reqs;

	spinlock_t		lock;

	unsigned long		private[];
};

static inline struct sdma *to_sdma(struct dma_device *d)
{
	return container_of(d, struct sdma, ddev);
}

static inline struct sdma_request *to_sdma_request(struct dma_chan *chan)
{
	return container_of(chan, struct sdma_request, vchan.chan);
}

static inline struct sdma_desc *to_sdma_desc(struct dma_async_tx_descriptor *tx)
{
	return container_of(tx, struct sdma_desc, vdesc.tx);
}

static inline void *sdma_priv(struct sdma *sdma)
{
	return (void*)sdma->private;
}

static inline void sdma_set_chan_private(struct sdma *sdma, void *ptr)
{
	int i;

	for (i = 0; i < sdma->channels_nr; i++) {
		struct sdma_channel *schan = &sdma->channels[i];

		schan->private = ptr;
	}
}

struct sdma_desc *sdma_report(struct sdma *sdma,
			      struct sdma_channel *chan,
			      enum sdma_report_status status);

struct sdma *sdma_alloc(struct device *dev,
			unsigned int channels,
			unsigned int requests,
			ssize_t lli_size,
			ssize_t priv_size);
void sdma_free(struct sdma *sdma);

int sdma_register(struct sdma *sdma,
		  struct sdma_ops *ops);
int sdma_unregister(struct sdma *sdma);

#endif /* _SCHEDULED_DMA_H_ */
