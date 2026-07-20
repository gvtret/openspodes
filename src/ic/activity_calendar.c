#include "activity_calendar.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t ac_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

/* ── Helper: copy season profile ────────────────────────────────────────── */

static void copy_season(osp_season_t *dst, const osp_season_t *src) {
	memcpy(dst->name, src->name, src->name_len);
	dst->name_len = src->name_len;
	dst->start = src->start;
	memcpy(dst->week_name, src->week_name, src->week_name_len);
	dst->week_name_len = src->week_name_len;
}

/* ── Helper: copy week profile ──────────────────────────────────────────── */

static void copy_week(osp_week_profile_t *dst, const osp_week_profile_t *src) {
	memcpy(dst->name, src->name, src->name_len);
	dst->name_len = src->name_len;
	for (int d = 0; d < 7; d++) {
		memcpy(dst->day_names[d], src->day_names[d], src->day_name_lens[d]);
		dst->day_name_lens[d] = src->day_name_lens[d];
	}
}

/* ── Helper: copy day profile ───────────────────────────────────────────── */

static void copy_day(osp_day_profile_t *dst, const osp_day_profile_t *src) {
	memcpy(dst->name, src->name, src->name_len);
	dst->name_len = src->name_len;
	dst->action_count = src->action_count;
	for (uint8_t a = 0; a < src->action_count; a++) {
		memcpy(&dst->actions[a], &src->actions[a], sizeof(osp_day_profile_action_t));
	}
}

/* ── get_attr ───────────────────────────────────────────────────────────── */

static osp_err_t ac_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_activity_calendar_t *a = (const osp_ic_activity_calendar_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &a->logical_name);
		case 2:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = a->calendar_name_active_len;
			memcpy(result->as.octetstring.data, a->calendar_name_active, a->calendar_name_active_len);
			return OSP_OK;
		case 3:
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.count = a->season_count_active;
			result->as.array.elements.capacity = OSP_MAX_SEASON_PROFILE;
			return OSP_OK;
		case 4:
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.count = a->week_count_active;
			result->as.array.elements.capacity = OSP_MAX_WEEK_PROFILE;
			return OSP_OK;
		case 5:
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.count = a->day_count_active;
			result->as.array.elements.capacity = OSP_MAX_DAY_PROFILE;
			return OSP_OK;
		case 6:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = a->calendar_name_passive_len;
			memcpy(result->as.octetstring.data, a->calendar_name_passive, a->calendar_name_passive_len);
			return OSP_OK;
		case 7:
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.count = a->season_count_passive;
			result->as.array.elements.capacity = OSP_MAX_SEASON_PROFILE;
			return OSP_OK;
		case 8:
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.count = a->week_count_passive;
			result->as.array.elements.capacity = OSP_MAX_WEEK_PROFILE;
			return OSP_OK;
		case 9:
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.count = a->day_count_passive;
			result->as.array.elements.capacity = OSP_MAX_DAY_PROFILE;
			return OSP_OK;
		case 10:
			result->tag = OSP_TAG_DATETIME;
			result->as.datetime = a->activate_passive_calendar_time;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

/* ── set_attr ───────────────────────────────────────────────────────────── */

static osp_err_t ac_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_activity_calendar_t *a = (osp_ic_activity_calendar_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len >= OSP_MAX_NAME_LEN) {
				return OSP_ERR_INVALID;
			}
			a->calendar_name_active_len = (uint8_t)value->as.octetstring.len;
			memcpy(a->calendar_name_active, value->as.octetstring.data, a->calendar_name_active_len);
			return OSP_OK;
		case 3:
			if (value->tag != OSP_TAG_ARRAY) return OSP_ERR_INVALID;
			a->season_count_active = value->as.array.elements.count;
			if (a->season_count_active > OSP_MAX_SEASON_PROFILE) a->season_count_active = OSP_MAX_SEASON_PROFILE;
			return OSP_OK;
		case 4:
			if (value->tag != OSP_TAG_ARRAY) return OSP_ERR_INVALID;
			a->week_count_active = value->as.array.elements.count;
			if (a->week_count_active > OSP_MAX_WEEK_PROFILE) a->week_count_active = OSP_MAX_WEEK_PROFILE;
			return OSP_OK;
		case 5:
			if (value->tag != OSP_TAG_ARRAY) return OSP_ERR_INVALID;
			a->day_count_active = value->as.array.elements.count;
			if (a->day_count_active > OSP_MAX_DAY_PROFILE) a->day_count_active = OSP_MAX_DAY_PROFILE;
			return OSP_OK;
		case 6:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len >= OSP_MAX_NAME_LEN) {
				return OSP_ERR_INVALID;
			}
			a->calendar_name_passive_len = (uint8_t)value->as.octetstring.len;
			memcpy(a->calendar_name_passive, value->as.octetstring.data, a->calendar_name_passive_len);
			return OSP_OK;
		case 7:
			if (value->tag != OSP_TAG_ARRAY) return OSP_ERR_INVALID;
			a->season_count_passive = value->as.array.elements.count;
			if (a->season_count_passive > OSP_MAX_SEASON_PROFILE) a->season_count_passive = OSP_MAX_SEASON_PROFILE;
			return OSP_OK;
		case 8:
			if (value->tag != OSP_TAG_ARRAY) return OSP_ERR_INVALID;
			a->week_count_passive = value->as.array.elements.count;
			if (a->week_count_passive > OSP_MAX_WEEK_PROFILE) a->week_count_passive = OSP_MAX_WEEK_PROFILE;
			return OSP_OK;
		case 9:
			if (value->tag != OSP_TAG_ARRAY) return OSP_ERR_INVALID;
			a->day_count_passive = value->as.array.elements.count;
			if (a->day_count_passive > OSP_MAX_DAY_PROFILE) a->day_count_passive = OSP_MAX_DAY_PROFILE;
			return OSP_OK;
		case 10:
			if (value->tag != OSP_TAG_DATETIME) return OSP_ERR_INVALID;
			a->activate_passive_calendar_time = value->as.datetime;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

/* ── invoke (method 1: activate_passive_calendar) ───────────────────────── */

static osp_err_t ac_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_activity_calendar_t *a = (osp_ic_activity_calendar_t *)inst;
	(void)param;
	*result = osp_val_null();

	if (method_id == 1) {
		/* Copy passive calendar to active */
		a->calendar_name_active_len = a->calendar_name_passive_len;
		memcpy(a->calendar_name_active, a->calendar_name_passive, a->calendar_name_active_len);

		a->season_count_active = a->season_count_passive;
		for (uint8_t i = 0; i < a->season_count_active; i++) {
			copy_season(&a->season_profile_active[i], &a->season_profile_passive[i]);
		}

		a->week_count_active = a->week_count_passive;
		for (uint8_t i = 0; i < a->week_count_active; i++) {
			copy_week(&a->week_profile_table_active[i], &a->week_profile_table_passive[i]);
		}

		a->day_count_active = a->day_count_passive;
		for (uint8_t i = 0; i < a->day_count_active; i++) {
			copy_day(&a->day_profile_table_active[i], &a->day_profile_table_passive[i]);
		}

		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

/* ── serialize ──────────────────────────────────────────────────────────── */

static osp_err_t ac_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_activity_calendar_class(), inst, buf, ac_attrs, 10);
}

/* ── deserialize ────────────────────────────────────────────────────────── */

static osp_err_t ac_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_activity_calendar_class(), inst, buf, ac_attrs, 10);
}

/* ── class definition ───────────────────────────────────────────────────── */

static const osp_ic_class_t ic_ac = {
    .name = "Activity Calendar",
    .class_id = 20,
    .version = 0,
    .get_attr = ac_get,
    .set_attr = ac_set,
    .invoke = ac_invoke,
    .serialize = ac_serialize,
    .deserialize = ac_deserialize,
    .instance_size = sizeof(osp_ic_activity_calendar_t),
};

const osp_ic_class_t *osp_ic_activity_calendar_class(void) {
	return &ic_ac;
}

void osp_ic_activity_calendar_init(osp_ic_activity_calendar_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
}
