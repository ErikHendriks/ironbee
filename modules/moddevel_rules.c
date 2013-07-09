/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- Development rules sub-module
 *
 * This is a module that defines some rule operators and actions for
 * development purposes.
 *
 * @note This module is enabled only for builds configured with
 * "--enable-devel".
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "moddevel_private.h"

#include <ironbee/action.h>
#include <ironbee/bytestr.h>
#include <ironbee/capture.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/list.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>


/**
 * Rule configuration
 */
struct ib_moddevel_rules_config_t {
    ib_list_t   *injection_list;  /**< List of rules to inject */
};

/**
 * Execute function for the "true" operator
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static ib_status_t op_true_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    *result = 1;

    if (capture != NULL && *result) {
        ib_capture_clear(capture);
        ib_capture_set_item(capture, 0, tx->mp, field);
    }
    return IB_OK;
}

/**
 * Execute function for the "false" operator
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static ib_status_t op_false_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    *result = 0;
    /* Don't check for capture, because we always return zero */

    return IB_OK;
}

/** Instance data for assert operator. */
struct assert_inst_data_t
{
    bool expand;
    const char *str;
};
typedef struct assert_inst_data_t assert_inst_data_t;

/**
 * Create function for the "assert" operator
 *
 * @param[in] ctx Current context.
 * @param[in] parameters Unparsed string with the parameters to
 *                       initialize the operator instance.
 * @param[out] instance_data Instance data.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static ib_status_t op_assert_create(
    ib_context_t  *ctx,
    const char    *parameters,
    void         **instance_data,
    void          *cbdata
)
{
    char *str;
    bool expand;
    ib_mpool_t *mp = ib_context_get_mpool(ctx);
    assert_inst_data_t *aid;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    aid = ib_mpool_alloc(mp, sizeof(*aid));
    if (aid == NULL) {
        return IB_EALLOC;
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        return IB_EALLOC;
    }

    /* Do we need expansion? */
    ib_data_expand_test_str(str, &expand);
    if (expand) {
        aid->expand = true;
    }

    aid->str = str;
    *instance_data = aid;

    return IB_OK;
}

/**
 * Execute function for the "assert" operator
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static ib_status_t op_assert_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    const assert_inst_data_t *aid = (const assert_inst_data_t *)instance_data;
    /* This works on C-style (NUL terminated) strings */
    const char *cstr = aid->str;
    char *expanded = NULL;
    ib_status_t rc;

    /* Expand the string */
    if (aid->expand) {
        rc = ib_data_expand_str(tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                            "log_execute: Failed to expand string '%s': %s",
                            cstr, ib_status_to_string(rc));
        }
    }
    else {
        expanded = (char *)cstr;
    }

    ib_log_error_tx(tx, "ASSERT: %s", expanded);
    assert(0 && expanded);
    return IB_OK;
}

/**
 * Execute function for the "exists" operator
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static ib_status_t op_exists_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    /* Return true of field is not NULL */
    *result = (field != NULL);

    if (capture != NULL && *result) {
        ib_capture_clear(capture);
        ib_capture_set_item(capture, 0, tx->mp, field);
    }

    return IB_OK;
}

/* IsType operators */
typedef enum
{
    IsTypeStr,
    IsTypeNulStr,
    IsTypeByteStr,
    IsTypeNum,
    IsTypeInt,
    IsTypeFloat,
} istype_t;

/* IsType operator data */
typedef struct
{
    istype_t    istype;
    int         numtypes;
    ib_ftype_t  types[2];
} istype_params_t;

/* IsType operators data */
static istype_params_t istype_params[] = {
    { IsTypeStr,     2, { IB_FTYPE_NULSTR, IB_FTYPE_BYTESTR } },
    { IsTypeNulStr,  1, { IB_FTYPE_NULSTR } },
    { IsTypeByteStr, 1, { IB_FTYPE_BYTESTR } },
    { IsTypeNum,     2, { IB_FTYPE_NUM, IB_FTYPE_FLOAT } },
    { IsTypeInt,     1, { IB_FTYPE_NUM } },
    { IsTypeFloat,   1, { IB_FTYPE_FLOAT } },
};

/**
 * Execute function for the "istype" operator family
 *
 * @note This operator is enabled only for builds configured with
 * "--enable-devel".
 *
 * @param[in] tx Current transaction.
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] field The field to operate on.
 * @param[in] capture If non-NULL, the collection to capture to.
 * @param[out] result The result of the operator 1=true 0=false.
 * @param[in] cbdata Callback data.
 *
 * @returns Status code
 */
static ib_status_t op_istype_execute(
    ib_tx_t *tx,
    void *instance_data,
    const ib_field_t *field,
    ib_field_t *capture,
    ib_num_t *result,
    void *cbdata
)
{
    assert(field != NULL);

    /* Ignore data */
    const istype_params_t *params = cbdata;
    int n;

    assert(params != NULL);

    *result = false;
    for (n = 0; n < params->numtypes; ++n) {
        if (params->types[n] == field->type) {
            *result = true;
        }
    }
    return IB_OK;
}

/**
 * Create function for the DebugLog action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t action_debuglog_create(
    ib_engine_t      *ib,
    const char       *parameters,
    ib_action_inst_t *inst,
    void             *cbdata)
{
    bool expand;
    char *str;
    ib_mpool_t *mp = ib_engine_pool_main_get(ib);

    assert(mp != NULL);

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        return IB_EALLOC;
    }

    /* Do we need expansion? */
    ib_data_expand_test_str(str, &expand);
    if (expand) {
        inst->flags |= IB_ACTINST_FLAG_EXPAND;
    }

    inst->data = str;
    return IB_OK;
}

/**
 * Execute function for the "debuglog" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data C-style string to log
 * @param[in] flags Action instance flags
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t action_debuglog_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    ib_flags_t            flags,
    void                 *cbdata)
{
    /* This works on C-style (NUL terminated) strings */
    const char *cstr = (const char *)data;
    char *expanded = NULL;
    ib_status_t rc;

    /* Expand the string */
    if ((flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        rc = ib_data_expand_str(rule_exec->tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec,
                              "log_execute: Failed to expand string '%s': %s",
                              cstr, ib_status_to_string(rc));
        }
    }
    else {
        expanded = (char *)cstr;
    }

    ib_rule_log_trace(rule_exec, "LOG: %s", expanded);
    return IB_OK;
}

/**
 * Create function for the 'print' action.
 *
 * @param[in] ib IronBee engine
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 */
static ib_status_t action_print_create(
    ib_engine_t      *ib,
    const char       *parameters,
    ib_action_inst_t *inst,
    void             *cbdata)
{
    char *str;
    bool expand;
    ib_mpool_t *mp = ib_engine_pool_main_get(ib);

    assert(mp != NULL);

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        return IB_EALLOC;
    }

    /* Do we need expansion? */
    ib_data_expand_test_str(str, &expand);
    if (expand) {
        inst->flags |= IB_ACTINST_FLAG_EXPAND;
    }

    inst->data = str;
    return IB_OK;
}

/**
 * Execute function for the "print" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data C-style string to log
 * @param[in] flags Action instance flags
 * @param[in] cbdata Unused.
 *
 * @returns Status code
 */
static ib_status_t action_print_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    ib_flags_t            flags,
    void                 *cbdata)
{
    const char *cstr = (const char *)data;
    char *expanded = NULL;
    ib_status_t rc;

    /* Expand the string */
    if ((flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        rc = ib_data_expand_str(rule_exec->tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec,
                              "print: Failed to expand string '%s': %d",
                              cstr, rc);
        }
    }
    else {
        expanded = (char *)cstr;
    }

    printf( "Rule %s => %s\n", ib_rule_id(rule_exec->rule), expanded);
    return IB_OK;
}

/**
 * Create function for the assert action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t action_assert_create(
    ib_engine_t      *ib,
    const char       *parameters,
    ib_action_inst_t *inst,
    void             *cbdata)
{
    bool expand;
    char *str;
    ib_mpool_t *mp = ib_engine_pool_main_get(ib);
    assert(mp != NULL);

    if (parameters == NULL) {
        parameters = "";
    }

    str = ib_mpool_strdup(mp, parameters);
    if (str == NULL) {
        return IB_EALLOC;
    }

    /* Do we need expansion? */
    ib_data_expand_test_str(str, &expand);
    if (expand) {
        inst->flags |= IB_ACTINST_FLAG_EXPAND;
    }

    inst->data = str;
    return IB_OK;
}

/**
 * Execute function for the "assert" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data C-style string to log
 * @param[in] flags Action instance flags
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t action_assert_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    ib_flags_t            flags,
    void                 *cbdata)
{
    /* This works on C-style (NUL terminated) strings */
    const char *cstr = (const char *)data;
    char *expanded = NULL;
    ib_status_t rc;

    /* Expand the string */
    if ((flags & IB_ACTINST_FLAG_EXPAND) != 0) {
        rc = ib_data_expand_str(rule_exec->tx->data, cstr, false, &expanded);
        if (rc != IB_OK) {
            ib_rule_log_error(rule_exec,
                              "log_execute: Failed to expand string '%s': %s",
                              cstr, ib_status_to_string(rc));
        }
    }
    else {
        expanded = (char *)cstr;
    }

    ib_rule_log_fatal(rule_exec, "ASSERT \"%s\"", expanded);
    return IB_OK;
}

/** Injection action functions & related declarations */
static const char *action_inject_name = "inject";

/**
 * Create function for the inject action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Callback data (configuration; unused)
 *
 * @returns Status code
 */
static ib_status_t action_inject_create_fn(
    ib_engine_t      *ib,
    const char       *parameters,
    ib_action_inst_t *inst,
    void             *cbdata)
{
    inst->data = NULL;
    return IB_OK;
}

/**
 * Inject action rule ownership function
 *
 * @param[in] ib IronBee engine
 * @param[in] rule Rule being registered
 * @param[in] cbdata Registration function callback data (configuration struct)
 *
 * @returns Status code:
 *   - IB_OK All OK, rule managed externally by module
 *   - IB_DECLINE Decline to manage rule
 *   - IB_Exx Other error
 */
static ib_status_t action_inject_ownership_fn(
    const ib_engine_t    *ib,
    const ib_rule_t      *rule,
    void                 *cbdata)
{
    assert(ib != NULL);
    assert(rule != NULL);
    assert(cbdata != NULL);

    ib_status_t rc;
    size_t count = 0;
    ib_moddevel_rules_config_t *config = (ib_moddevel_rules_config_t *)cbdata;

    rc = ib_rule_search_action(ib, rule, IB_RULE_ACTION_TRUE,
                               action_inject_name,
                               NULL, &count);
    if (rc != IB_OK) {
        return rc;
    }
    if (count == 0) {
        return IB_DECLINED;
    }
    rc = ib_list_push(config->injection_list, (ib_rule_t *)rule);
    if (rc != IB_OK) {
        return rc;
    }
    return IB_OK;
}

/**
 * Inject action rule injection function
 *
 * @param[in] ib IronBee engine
 * @param[in] rule_exec Rule execution environment
 * @param[in,out] rule_list List of rules to execute (append-only)
 * @param[in] cbdata Injection function callback data (configuration struct)
 *
 * @returns Status code:
 *   - IB_OK All OK
 *   - IB_Exx Other error
 */
static ib_status_t action_inject_injection_fn(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata)
{
    assert(ib != NULL);
    assert(rule_exec != NULL);
    assert(rule_list != NULL);
    assert(cbdata != NULL);

    const ib_list_node_t *node;
    const ib_moddevel_rules_config_t *config =
        (const ib_moddevel_rules_config_t *)cbdata;

    IB_LIST_LOOP_CONST(config->injection_list, node) {
        const ib_rule_t *rule = (const ib_rule_t *)node->data;
        if (rule->meta.phase == rule_exec->phase) {
            ib_status_t rc = ib_list_push(rule_list, (ib_rule_t *)rule);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }
    return IB_OK;
}

ib_status_t ib_moddevel_rules_init(
    ib_engine_t                 *ib,
    ib_module_t                 *mod,
    ib_mpool_t                  *mp,
    ib_moddevel_rules_config_t **pconfig)
{
    ib_status_t rc;
    ib_rule_phase_num_t       phase;
    ib_moddevel_rules_config_t *config;
    ib_log_debug(ib, "Initializing rule development module");

    /* Allocate a configuration object */
    config = ib_mpool_calloc(mp, sizeof(*config), 1);
    if (config == NULL) {
        return IB_EALLOC;
    }
    rc = ib_list_create(&(config->injection_list), mp);
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * Simple True / False operators.
     */

    /* Register the true operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "true",
        ( IB_OP_CAPABILITY_ALLOW_NULL |
          IB_OP_CAPABILITY_NON_STREAM |
              IB_OP_CAPABILITY_STREAM |
        IB_OP_CAPABILITY_CAPTURE ),
        NULL, NULL, /* No create function */
        NULL, NULL, /* no destroy function */
        op_true_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the false operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "false",
        ( IB_OP_CAPABILITY_ALLOW_NULL |
          IB_OP_CAPABILITY_NON_STREAM |
              IB_OP_CAPABILITY_STREAM ),
        NULL, NULL, /* No create function */
        NULL, NULL, /* no destroy function
    */
                              op_false_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the field exists operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "exists",
        ( IB_OP_CAPABILITY_ALLOW_NULL |
          IB_OP_CAPABILITY_NON_STREAM |
          IB_OP_CAPABILITY_CAPTURE ),
        NULL, NULL, /* No create function */
        NULL, NULL, /* no destroy function */
        op_exists_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the false operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "assert",
        ( IB_OP_CAPABILITY_ALLOW_NULL |
          IB_OP_CAPABILITY_NON_STREAM |
              IB_OP_CAPABILITY_STREAM ),
        op_assert_create, NULL,
        NULL, NULL, /* no destroy function
    */
                              op_assert_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * IsType operators
     */

    /* Register the IsStr operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "IsStr",
        ( IB_OP_CAPABILITY_NON_STREAM |
          IB_OP_CAPABILITY_STREAM ),
        NULL, NULL, /* no create function */
        NULL, NULL, /* no destroy function */
        op_istype_execute, &istype_params[IsTypeStr]
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsNulStr operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "IsNulStr",
        ( IB_OP_CAPABILITY_NON_STREAM |
          IB_OP_CAPABILITY_STREAM ),
        NULL, NULL, /* no create function */
        NULL, NULL, /* no destroy function */
        op_istype_execute, &istype_params[IsTypeNulStr]
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsByteStr operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "IsByteStr",
        ( IB_OP_CAPABILITY_NON_STREAM |
          IB_OP_CAPABILITY_STREAM ),
        NULL, NULL, /* no create function */
        NULL, NULL, /* no destroy function */
        op_istype_execute, &istype_params[IsTypeByteStr]
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsNum operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "IsNum",
        ( IB_OP_CAPABILITY_NON_STREAM |
          IB_OP_CAPABILITY_STREAM ),
        NULL, NULL, /* no create function */
        NULL, NULL, /* no destroy function */
        op_istype_execute, &istype_params[IsTypeNum]
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsInt operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "IsInt",
        ( IB_OP_CAPABILITY_NON_STREAM |
          IB_OP_CAPABILITY_STREAM ),
        NULL, NULL, /* no create function */
        NULL, NULL, /* no destroy function */
        op_istype_execute, &istype_params[IsTypeInt]
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the IsFloat operator */
    rc = ib_operator_create_and_register(
        NULL,
        ib,
        "IsFloat",
        ( IB_OP_CAPABILITY_NON_STREAM |
          IB_OP_CAPABILITY_STREAM ),
        NULL, NULL, /* no create function */
        NULL, NULL, /* no destroy function */
        op_istype_execute, &istype_params[IsTypeFloat]
    );
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * Debug logging actions
     */

    /* Register the DebugLog action */
    rc = ib_action_register(ib,
                            "DebugLog",
                            IB_ACT_FLAG_NONE,
                            action_debuglog_create, NULL,
                            NULL, NULL, /* no destroy function */
                            action_debuglog_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the Print action */
    rc = ib_action_register(ib,
                            "Print",
                            IB_ACT_FLAG_NONE,
                            action_print_create, NULL,
                            NULL, NULL, /* no destroy function */
                            action_print_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }


    /**
     * Assert action
     */

    /* Register the assert action */
    rc = ib_action_register(ib,
                            "assert",
                            IB_ACT_FLAG_NONE,
                            action_assert_create, NULL,
                            NULL, NULL, /* no destroy function */
                            action_assert_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /**
     * Inject action and related rule engine callbacks
     */

    /* Register the inject action */
    rc = ib_action_register(ib,
                            action_inject_name,
                            IB_ACT_FLAG_NONE,
                            action_inject_create_fn, config,
                            NULL, NULL, /* no destroy function */
                            NULL, NULL); /* no execute function */
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the ownership function */
    rc = ib_rule_register_ownership_fn(ib, action_inject_name,
                                       action_inject_ownership_fn, config);
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the injection function */
    for (phase = IB_PHASE_NONE; phase < IB_RULE_PHASE_COUNT; ++phase) {
        rc = ib_rule_register_injection_fn(ib, action_inject_name,
                                           phase,
                                           action_inject_injection_fn, config);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

ib_status_t ib_moddevel_rules_cleanup(
    ib_engine_t                *ib,
    ib_module_t                *mod,
    ib_moddevel_rules_config_t *config)
{
    /* Do nothing */
    return IB_OK;
}

ib_status_t ib_moddevel_rules_fini(
    ib_engine_t                *ib,
    ib_module_t                *mod)
{
    return IB_OK;
}
