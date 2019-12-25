/*
 * SC-Controller - Gyro, GyroAbs and Accel
 *
 * 'gyro' uses *relative* gyroscope position as input for emulated axes.
 * 'gyroabs' sets axis position based on absolute rotation.
 * 'accel' sets axis position based on absolute position,
 *         as random number given by accelerator
 */
#include "scc/utils/logging.h"
#include "scc/utils/strbuilder.h"
#include "scc/utils/assert.h"
#include "scc/utils/math.h"
#include "scc/utils/rc.h"
#include "scc/param_checker.h"
#include "scc/conversions.h"
#include "scc/action.h"
#include "wholehaptic.h"
#include "tostring.h"
#include "internal.h"
#include "props.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if REL_X == 0
#error "WTF"
#endif

static const char* KW_GYRO = "gyro";
const char* KW_GYROABS = "gyroabs";
static const char* KW_ACCEL = "accel";
// Just random number to put default sensitivity into sane range
static const double MOUSE_FACTOR = 0.01;

typedef struct {
	ParameterList		params;
	Action				action;
	Axis				axes[3];
	bool				was_out_of_range;
	double				sensitivity[3];
	double				ir[4];
	Action*				deadzone;
	HapticData			hdata;
} GyroAction;


ACTION_MAKE_TO_STRING(GyroAction, gyro, _a->type, NULL);

static char* describe(Action* a, ActionDescContext ctx) {
	GyroAction* g = container_of(a, GyroAction, action);
	
	LOG(">>> %i %i", REL_X, REL_MAX);
	if ((g->axes[0] >= REL_X) && (g->axes[0] <= REL_MAX))
		return strbuilder_cpy("Mouse");
	
	StrBuilder* sb = strbuilder_new();
	if (sb == NULL) return NULL;
	
	char* descs[3] = { NULL, NULL, NULL };
	for (int i=0; i<3; i++)
		descs[i] = scc_describe_axis(g->axes[i], 0);
	
	bool joined = strbuilder_join(sb, (const char**)descs, 3, "\n");
	for (int i=0; i<3; i++)
		free(descs[i]);
	
	if (joined) {
		return strbuilder_consume(sb);
	} else {
		strbuilder_free(sb);
		return NULL;
	}
}

static void gyro_dealloc(Action* a) {
	GyroAction* g = container_of(a, GyroAction, action);
	list_free(g->params);
	RC_REL(g->deadzone);
	free(g);
}


static void set_sensitivity(Action* a, float x, float y, float z) {
	GyroAction* g = container_of(a, GyroAction, action);
	g->sensitivity[0] = x;
	g->sensitivity[1] = y;
	g->sensitivity[2] = z;
}

static void set_haptic(Action* a, HapticData hdata) {
	GyroAction* g = container_of(a, GyroAction, action);
	g->hdata = hdata;
}

static void gyro(Action* a, Mapper* m, const struct GyroInput* value) {
	GyroAction* g = container_of(a, GyroAction, action);
	const GyroValue* pyr = &value->gpitch;
	
	for (int i=0; i<3; i++) {
		if (g->axes[i] < ABS_CNT) {
			double v = (double)pyr[i] * g->sensitivity[i] * -10.0;
			m->set_axis(m, g->axes[i], clamp(STICK_PAD_MIN, v, STICK_PAD_MAX));
		}
	}
}

void scc_gyroabs_set_deadzone_mod(Action* a, Action* deadzone) {
	ASSERT(a->type == KW_GYROABS);
	GyroAction* g = container_of(a, GyroAction, action);
	RC_ADD(deadzone);
	RC_REL(g->deadzone);
	g->deadzone = deadzone;
}

static void gyroabs(Action* a, Mapper* m, const struct GyroInput* value) {
	GyroAction* g = container_of(a, GyroAction, action);
	static const double MAGIC = 10430.378350470453;	// (2^15) / PI
	double pyr[3];
	const ControllerFlags flags = m->get_flags(m);
	
	if ((flags & CF_EUREL_GYROS) != 0) {
		pyr[0] = value->q0 / MAGIC;
		pyr[1] = value->q1 / MAGIC;
		pyr[2] = value->q2 / MAGIC;
	} else {
		quat2euler(pyr, value->q0 / 32768.0, value->q1 / 32768.0,
						value->q2 / 32768.0, value->q3 / 32768.0);
	}
	
	for (int i=0; i<3; i++) {
		g->ir[i] = g->ir[i] || pyr[i];
		pyr[i] = anglediff(g->ir[i], pyr[i]) * g->sensitivity[i] * MAGIC * 2;
	}
	if (HAPTIC_ENABLED(&g->hdata)) {
		bool out_of_range = false;
		for (int i=0; i<3; i++) {
			pyr[i] = floor(pyr[i]);
			if (pyr[i] > STICK_PAD_MAX) {
				pyr[i] = STICK_PAD_MAX;
				out_of_range = true;
			} else if (pyr[i] < STICK_PAD_MIN) {
				pyr[i] = STICK_PAD_MIN;
				out_of_range = true;
			}
		}
		if (out_of_range) {
			if (!g->was_out_of_range) {
				m->haptic_effect(m, &g->hdata);
				g->was_out_of_range = true;
			}
		} else {
				g->was_out_of_range = true;
		}
	} else {
		for (int i=0; i<3; i++) {
			pyr[i] = clamp(STICK_PAD_MIN, pyr[i], STICK_PAD_MAX);
		}
	}
	
	for (int i=0; i<3; i++) {
		Axis axis = g->axes[i];
		if (axis == REL_X) {
			m->move_mouse(m, clamp_axis(axis, pyr[i] * MOUSE_FACTOR * g->sensitivity[i]), 0);
		} else if (axis == REL_Y) {
			m->move_mouse(m, 0, clamp_axis(axis, pyr[i] * MOUSE_FACTOR * g->sensitivity[i]));
		} else if (axis < ABS_CNT) {
			AxisValue val = clamp_axis(axis, pyr[i] * g->sensitivity[i]);
			if (g->deadzone != NULL) {
				scc_deadzone_apply(g->deadzone, &val);
			}
			m->set_axis(m, axis, val);
		}
	}
}

static void accel(Action* a, Mapper* m, const struct GyroInput* value) {
	GyroAction* g = container_of(a, GyroAction, action);
	GyroValue* xyz = &value->accel_x;
	
	for (int i=0; i<3; i++) {
		Axis axis = g->axes[i];
		if (axis == REL_X) {
			m->move_mouse(m, clamp_axis(axis, xyz[i] * MOUSE_FACTOR * g->sensitivity[i]), 0);
		} else if (axis == REL_Y) {
			m->move_mouse(m, 0, clamp_axis(axis, xyz[i] * MOUSE_FACTOR * g->sensitivity[i]));
		} else if (axis < ABS_CNT) {
			AxisValue val = clamp_axis(axis, xyz[i] * g->sensitivity[i]);
			if (g->deadzone != NULL) {
				scc_deadzone_apply(g->deadzone, &val);
			}
			m->set_axis(m, axis, val);
		}
	}
}

static Parameter* get_property(Action* a, const char* name) {
	GyroAction* g = container_of(a, GyroAction, action);
	if (0 == strcmp(name, "sensitivity")) {
		Parameter* xyz[] = {
			scc_new_float_parameter(g->sensitivity[0]),
			scc_new_float_parameter(g->sensitivity[1]),
			scc_new_float_parameter(g->sensitivity[2])
		};
		if ((xyz[0] == NULL) || (xyz[1] == NULL) || (xyz[2] == NULL)) {
			free(xyz[0]); free(xyz[1]); free(xyz[2]);
			return NULL;
		}
		return scc_new_tuple_parameter(3, xyz);
	}
	if (0 == strcmp(name, "axes")) {
		Parameter* xyz[] = {
			scc_new_int_parameter(g->axes[0]),
			scc_new_int_parameter(g->axes[1]),
			scc_new_int_parameter(g->axes[2])
		};
		if ((xyz[0] == NULL) || (xyz[1] == NULL) || (xyz[2] == NULL)) {
			free(xyz[0]); free(xyz[1]); free(xyz[2]);
			return NULL;
		}
		return scc_new_tuple_parameter(3, xyz);
	}
	MAKE_HAPTIC_PROPERTY(g->hdata, "haptic");
	
	DWARN("Requested unknown property '%s' from '%s'", name, a->type);
	return NULL;
}


static ActionOE gyro_constructor(const char* keyword, ParameterList params) {
	// gyro doesn't use ParamChecker, as it allows either axis (number) or None
	// for any of one to three parameters.
	if ((list_len(params) < 1) || (list_len(params) > 3)) {
		return (ActionOE)scc_new_param_error(AEC_INVALID_NUMBER_OF_PARAMETERS,
							"Invalid number of parameters for '%s'", keyword);
	}
	Axis axes[3] = { ABS_CNT, ABS_CNT, ABS_CNT };
	for (int i=0; i<3; i++) {
		if (i >= list_len(params))
			break;
		if (scc_parameter_type(list_get(params, i)) == PT_NONE)
			continue;
		if ((scc_parameter_type(list_get(params, i)) & PT_INT) == 0)
			return (ActionOE)scc_new_invalid_parameter_type_error(keyword, i, list_get(params, i));
		axes[i] = scc_parameter_as_int(list_get(params, i));
	}
	
	params = scc_copy_param_list(params);
	if (params == NULL) return (ActionOE)scc_oom_action_error();
	
	GyroAction* g = malloc(sizeof(GyroAction));
	if (g == NULL) return (ActionOE)scc_oom_action_error();
	
	if (strcmp(keyword, KW_GYRO) == 0) {
		scc_action_init(&g->action, KW_GYRO,
						AF_ACTION | AF_MOD_SENSITIVITY | AF_MOD_SENS_Z,
						&gyro_dealloc, &gyro_to_string);
		g->action.gyro = &gyro;
	} else if (strcmp(keyword, KW_GYROABS) == 0) {
		scc_action_init(&g->action, KW_GYROABS,
						AF_MOD_DEADZONE | AF_ACTION | AF_MOD_SENSITIVITY | AF_MOD_SENS_Z,
						&gyro_dealloc, &gyro_to_string);
		g->action.gyro = &gyroabs;
	} else {
		scc_action_init(&g->action, KW_ACCEL,
						AF_MOD_DEADZONE | AF_ACTION | AF_MOD_SENSITIVITY | AF_MOD_SENS_Z,
						&gyro_dealloc, &gyro_to_string);
		g->action.gyro = &accel;
	}
	
	g->action.describe = &describe;
	g->action.extended.set_sensitivity = &set_sensitivity;
	g->action.get_property = &get_property;
	g->action.extended.set_haptic = &set_haptic;
	
	for (int i=0; i<3; i++) {
		g->axes[i] = axes[i];
		g->sensitivity[i] = 1.0;
	}
	g->ir[0] = g->ir[1] = g->ir[2] = g->ir[3] = 0;
	g->was_out_of_range = false;
	g->deadzone = NULL;
	HAPTIC_DISABLE(&g->hdata);
	g->params = params;
	
	return (ActionOE)&g->action;
}

void scc_actions_init_gyro() {
	scc_action_register(KW_GYRO, &gyro_constructor);
	scc_action_register(KW_GYROABS, &gyro_constructor);
	scc_action_register(KW_ACCEL, &gyro_constructor);
}

