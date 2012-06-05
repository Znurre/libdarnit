#ifndef HAVE_GLES
	#include "render_ogl.c"
#else
	#include "render_gles.c"	
#endif


int renderInit(void *handle) {
	DARNIT *m = handle;

	m->tsr.cnt = 0;
	m->tsr.tsr = NULL;

	return 0;
}


int renderAddTSRef(void *handle, const char *fname, TILESHEET *ts) {
	DARNIT *m = handle;
	void *tmp;
	int i, len, fast_cmp;

	if (fname == NULL) return -1;
	len = strlen(fname);

	for (i = fast_cmp = 0; i < len; i++)
		fast_cmp += fname[i];
	
	for (i = 0; i < m->tsr.cnt; i++)
		if (m->tsr.tsr[i].fast_cmp == 0)
			break;
	
	if (i == m->tsr.cnt) {
		m->tsr.cnt++;
		if ((tmp = realloc(m->tsr.tsr, sizeof(TILESHEET_REF) * m->tsr.cnt)) == NULL) {
			m->tsr.cnt--;
			return -1;
		}
		m->tsr.tsr = tmp;
	}

	if ((m->tsr.tsr[i].fname = malloc(strlen(fname)+1)) == NULL) {
		m->tsr.tsr[i].fast_cmp = 0;
		return -1;
	}

	strcpy(m->tsr.tsr[i].fname, fname);
	m->tsr.tsr[i].fast_cmp = fast_cmp;
	m->tsr.tsr[i].ref = ts;

	return i;
}
	

void renderDelTSRef(void *handle, int ref) {
	DARNIT *m = handle;

	if (ref < 0 || ref >= m->tsr.cnt)
		return;
	
	m->tsr.tsr[ref].fast_cmp = 0;

	return;
}


TILESHEET *renderGetTilesheetFromRef(void *handle, const char *fname) {
	DARNIT *m = handle;
	int i, len, fast_cmp;

	if (fname == NULL) return NULL;
	len = strlen(fname);

	for (i = fast_cmp = 0; i < len; i++)
		fast_cmp += fname[i];
	
	for (i = 0; i < m->tsr.cnt; i++) {
		if (m->tsr.tsr[i].fast_cmp == fast_cmp)
			if (strcmp(m->tsr.tsr[i].fname, fname) == 0) {
				m->tsr.tsr[i].ref->ref_count++;
				return m->tsr.tsr[i].ref;
			}
	}

	return NULL;
}


TILESHEET *renderTilesheetLoad(void *handle, const char *fname, unsigned int wsq, unsigned int hsq, unsigned int convert_to) {
	DARNIT *m = handle;
	TILESHEET *ts;
	IMGLOAD_DATA data;
	void *data_t;
	float twgran, thgran;
	int i;

	if ((ts = renderGetTilesheetFromRef(handle, fname)) != NULL)
		return ts;

	if ((ts = malloc(sizeof(TILESHEET))) == NULL) {
		MALLOC_ERROR
		return NULL;
	}
	
	data = imgloadLoad(fname);
	if (data.img_data == NULL) {
		free(ts);
		return NULL;
	}

	ts->w = data.w, ts->h = data.h, data_t = data.img_data;
	
/*	#ifndef HAVE_GLES */
		ts->texhandle = videoAddTexture(data_t, ts->w, ts->h);
/*	#else
		imgloadDownsample(&data, convert_to);
		data_t = data.img_data;
		if (convert_to == PFORMAT_RGBA8)
			ts->texhandle = videoAddTexture(data_t, ts->w, ts->h);
		else if (convert_to == PFORMAT_RGBA4)
			ts->texhandle = videoAddTextureRGBA4(data_t, ts->w, ts->h);
		else if (convert_to == PFORMAT_RGB5A1)
			ts->texhandle = videoAddTextureRGB5A1(data_t, ts->w, ts->h);
	#endif*/
	
	free(data_t);

	twgran = 1.0f / ts->w * wsq;
	thgran = 1.0f / ts->h * hsq;

	ts->sw = m->video.swgran * wsq;
	ts->sh = m->video.shgran * hsq;
	ts->swgran = m->video.swgran;
	ts->shgran = m->video.shgran;
	ts->wsq = wsq;
	ts->hsq = hsq;

	ts->tiles = (ts->w / wsq) * (ts->h / hsq);
	if ((ts->tile = malloc(ts->tiles * sizeof(TILE))) == NULL) {
		MALLOC_ERROR
		videoRemoveTexture(ts->texhandle);
		free(ts);
		return NULL;
	}

	for (i = 0; i < ts->tiles; i++) {
		ts->tile[i].r = twgran * (i % (ts->w / wsq));
		ts->tile[i].s = thgran * (i / (ts->w / wsq));
		ts->tile[i].u = ts->tile[i].r + twgran;
		ts->tile[i].v = ts->tile[i].s + thgran;
	}

	renderAddTSRef(handle, fname, ts);
	ts->ref_count = 0;

	return ts;
}


void *renderTilesheetFree(void *handle, TILESHEET *ts) {
	DARNIT *m = handle;
	if (ts == NULL) return NULL;

	ts->ref_count--;
	if (ts->ref_count > 0) return ts;
	videoRemoveTexture(ts->texhandle);
	renderDelTSRef(m, ts->ref);
	free(ts->tile);
	free(ts);

	return NULL;
}


void renderCalcTilePosCache(TILE_CACHE *cache, TILESHEET *ts, float x, float y) {
	x /= ts->wsq;
	y /= ts->hsq;
	cache->x = (x*ts->sw) - 1.0f;
	cache->y = 1.0f-(y*ts->sh);
	cache->x2 = (ts->sw + x*ts->sw) - 1.0f;
	cache->y2 = 1.0f-(y*ts->sh);
	cache->x3 = (ts->sw + x*ts->sw-1.0f);
	cache->y3 = (1.0f-y*ts->sh - ts->sh);
	cache->x4 = (ts->sw + x*ts->sw-1.0f);
	cache->y4 = (1.0f-y*ts->sh - ts->sh);
	cache->x5 = (x*ts->sw-1.0f);
	cache->y5 = (1.0f-y*ts->sh - ts->sh);
	cache->x6 = (x*ts->sw-1.0f);
	cache->y6 = (1.0f-y*ts->sh);

	return;
}


void renderCalcTileCache(TILE_CACHE *cache, TILESHEET *ts, unsigned int tile) {
	unsigned int t = tile;

	cache->u = ts->tile[t].r;
	cache->v = ts->tile[t].s;
	cache->u2 = ts->tile[t].u;
	cache->v2 = ts->tile[t].s;
	cache->u3 = ts->tile[t].u;
	cache->v3 = ts->tile[t].v;
	cache->u4 = ts->tile[t].u;
	cache->v4 = ts->tile[t].v;
	cache->u5 = ts->tile[t].r;
	cache->v5 = ts->tile[t].v;
	cache->u6 = ts->tile[t].r;
	cache->v6 = ts->tile[t].s;

	return;
}


void renderSetTileCoord(TILE_CACHE *cache, TILESHEET *ts, unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
	float xf, yf, wf, hf, tw, th;
	
	tw = 1.0f / ts->w;
	th = 1.0f / ts->h;
	xf = tw * x;
	yf = th * y;
	wf = tw * w;
	hf = th * h;
	wf += xf;
	hf += yf;

	cache->u = xf;
	cache->v = yf;
	cache->u2 = wf;
	cache->v2 = yf;
	cache->u3 = wf;
	cache->v3 = hf;
	cache->u4 = wf;
	cache->v4 = hf;
	cache->u5 = xf;
	cache->v5 = hf;
	cache->u6 = xf;
	cache->v6 = yf;
	
	return;
}


void renderCache(TILE_CACHE *cache, TILESHEET *ts, int tiles) {
	if (cache == NULL) return;
	glBindTexture(GL_TEXTURE_2D, ts->texhandle);

	glVertexPointer(2, GL_FLOAT, 16, cache);
	glTexCoordPointer(2, GL_FLOAT, 16, &cache->u);

	glDrawArrays(GL_TRIANGLES, 0, tiles*6);

	return;
}
