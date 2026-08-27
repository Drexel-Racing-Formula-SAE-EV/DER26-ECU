/**
 * @file canbus.c
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-24
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <string.h>

#include "ext_drivers/canbus.h"
#include "ext_drivers/ams.h"
#include "ext_drivers/cm200.h"
#include "task.h"

#define CANBUS_CAN1_FILTER_BANK_FIRST 0u
#define CANBUS_CAN1_FILTER_BANK_SPLIT 14u
#define CANBUS_FILTER_BANK_COUNT      7u
#define CANBUS_AMS_RANGE_FILTER_BANK  CANBUS_FILTER_BANK_COUNT
#define CANBUS_IDS_PER_16BIT_LIST_BANK 4u

_Static_assert(CANBUS_CAN1_FILTER_BANK_FIRST < CANBUS_CAN1_FILTER_BANK_SPLIT,
               "CAN1 filter ownership must remain below the CAN2 split");
_Static_assert(CANBUS_AMS_RANGE_FILTER_BANK < CANBUS_CAN1_FILTER_BANK_SPLIT,
               "ECU CAN1 filters must fit in banks 0..13");
#define CANBUS_STD_ID_FILTER_VALUE(id) ((uint32_t)((id) << 5u))

static HAL_StatusTypeDef canbus_configure_rx_filters(CAN_HandleTypeDef *hcan)
{
    static const uint16_t allowed_ids[CANBUS_FILTER_BANK_COUNT * CANBUS_IDS_PER_16BIT_LIST_BANK] = {
        /* DER26-CAN-V4 retires legacy 0x069 bulk telemetry. Keep every
         * explicit list slot safe by using valid protected/detail IDs only. */
        AMS_ECU_STATUS_CANBUS_ID,
        AMS_ESTIMATOR_CANBUS_ID,
        AMS_ECU_STATUS_CANBUS_ID,
        AMS_ECU_ELECTRICAL_CANBUS_ID,
        AMS_ECU_THERMAL_CANBUS_ID,
        AMS_ECU_HEALTH_CANBUS_ID,
        AMS_ECU_CURRENT_DIAG_CANBUS_ID,
        DER26_POWER_DCL_ID,
        DER26_POWER_CCL_ID,
        DER26_POWER_SOH_ID,
        DER26_POWER_ENVELOPE_ID,
        DER26_POWER_STRATEGY_ID,
        DER26_POWER_BINDINGS_ID,
        CM200_TEMPERATURES_1_CAN_ID,
        CM200_TEMPERATURES_2_CAN_ID,
        CM200_TEMPERATURES_3_CAN_ID,
        CM200_MOTOR_POSITION_CAN_ID,
        CM200_CURRENT_CAN_ID,
        CM200_VOLTAGE_CAN_ID,
        CM200_INTERNAL_STATES_CAN_ID,
        CM200_FAULTS_CAN_ID,
        CM200_TORQUE_TIMER_CAN_ID,
        CM200_FIRMWARE_CAN_ID,
        CM200_TORQUE_CAP_CAN_ID,
        /* Explicit safe duplicates fill all otherwise-unused list slots.
         * Do not rely on implicit zero initialization: that would program
         * acceptance filters for standard ID 0x000. */
        DER26_POWER_DCL_ID,
        AMS_ECU_STATUS_CANBUS_ID,
        CM200_INTERNAL_STATES_CAN_ID,
        DER26_POWER_CCL_ID,
    };
    _Static_assert((sizeof(allowed_ids) / sizeof(allowed_ids[0])) ==
                   (CANBUS_FILTER_BANK_COUNT * CANBUS_IDS_PER_16BIT_LIST_BANK),
                   "CAN filter list must explicitly fill every hardware slot");

    if(hcan == NULL)
    {
        return HAL_ERROR;
    }

    for(uint32_t bank = 0u; bank < CANBUS_FILTER_BANK_COUNT; bank++)
    {
        const uint32_t base = bank * CANBUS_IDS_PER_16BIT_LIST_BANK;
        CAN_FilterTypeDef filter = {0};
        filter.FilterBank = CANBUS_CAN1_FILTER_BANK_FIRST + bank;
        filter.FilterMode = CAN_FILTERMODE_IDLIST;
        filter.FilterScale = CAN_FILTERSCALE_16BIT;
        filter.FilterIdHigh = CANBUS_STD_ID_FILTER_VALUE(allowed_ids[base + 0u]);
        filter.FilterIdLow = CANBUS_STD_ID_FILTER_VALUE(allowed_ids[base + 1u]);
        filter.FilterMaskIdHigh = CANBUS_STD_ID_FILTER_VALUE(allowed_ids[base + 2u]);
        filter.FilterMaskIdLow = CANBUS_STD_ID_FILTER_VALUE(allowed_ids[base + 3u]);
        filter.FilterFIFOAssignment = CAN_RX_FIFO0;
        filter.FilterActivation = ENABLE;
        filter.SlaveStartFilterBank = CANBUS_CAN1_FILTER_BANK_SPLIT;
        if(HAL_CAN_ConfigFilter(hcan, &filter) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    /* One range filter captures every passive AMS logger frame without
     * consuming dozens of exact-ID slots. Standard data frames 0x680-0x6FF
     * match; extended and remote frames are rejected by the low-word mask. */
    {
        CAN_FilterTypeDef filter = {0};
        filter.FilterBank = CANBUS_AMS_RANGE_FILTER_BANK;
        filter.FilterMode = CAN_FILTERMODE_IDMASK;
        filter.FilterScale = CAN_FILTERSCALE_32BIT;
        filter.FilterIdHigh = CANBUS_STD_ID_FILTER_VALUE(0x680u);
        filter.FilterIdLow = 0u;
        filter.FilterMaskIdHigh = CANBUS_STD_ID_FILTER_VALUE(0x780u);
        filter.FilterMaskIdLow = 0x0006u; /* IDE=standard, RTR=data */
        filter.FilterFIFOAssignment = CAN_RX_FIFO0;
        filter.FilterActivation = ENABLE;
        filter.SlaveStartFilterBank = CANBUS_CAN1_FILTER_BANK_SPLIT;
        if(HAL_CAN_ConfigFilter(hcan, &filter) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }
    return HAL_OK;
}

void canbus_device_init(canbus_t *dev, CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *tx_header)
{
    if((dev == NULL) || (hcan == NULL) || (tx_header == NULL))
    {
        return;
    }

    dev->hcan = hcan;
    dev->tx_header = tx_header;
    dev->tx_dropped_count = 0u;
    dev->tx_replaced_count = 0u;
    dev->rx_accepted_count = 0u;
    dev->rx_ignored_count = 0u;
    dev->rx_malformed_count = 0u;
    dev->rx_remote_count = 0u;
    dev->feedback_tx_count = 0u;
    dev->feedback_tx_deferred_count = 0u;
    dev->feedback_tx_error_count = 0u;
    dev->feedback_last_tx_tick = 0u;
    dev->tx_wait_count = 0u;
    dev->tx_wait_timeout_count = 0u;
    dev->tx_wait_last_cycles = 0u;
    dev->tx_wait_max_cycles = 0u;
    dev->tx_pending_mailbox_mask = 0u;
    dev->tx_complete_mailbox_mask = 0u;
    dev->tx_abort_mailbox_mask = 0u;
    dev->tx_complete_count = 0u;
    dev->tx_abort_count = 0u;
    dev->tx_complete_timeout_count = 0u;
    dev->tx_unexpected_callback_count = 0u;
    dev->tx_outcome_uncertain_latched = false;
    dev->filters_configured = false;
    dev->started = false;
    dev->tx_queue = xQueueCreateStatic(CANBUS_TX_QUEUE_LENGTH,
        sizeof(canbus_tx_request_t), dev->tx_queue_storage,
        &dev->tx_queue_control);

    dev->tx_header->IDE = CAN_ID_STD;
    dev->tx_header->StdId = 0x00;
    dev->tx_header->ExtId = 0x00;
    dev->tx_header->RTR = CAN_RTR_DATA;
    dev->tx_header->DLC = 8;
    dev->tx_header->TransmitGlobalTime = DISABLE;

    dev->filters_configured = (canbus_configure_rx_filters(hcan) == HAL_OK);
    dev->started = ((dev->tx_queue != NULL) &&
                    dev->filters_configured &&
                    (HAL_CAN_Start(hcan) == HAL_OK));
}

HAL_StatusTypeDef canbus_queue_tx(canbus_t *dev, const canbus_tx_request_t *request)
{
    if((dev == NULL) || (request == NULL) || (dev->tx_queue == NULL))
    {
        return HAL_ERROR;
    }

    if(uxQueueMessagesWaiting(dev->tx_queue) != 0u)
    {
        dev->tx_replaced_count++;
    }

    /* This is a latest-value mailbox, not a FIFO.  A newer disable request
     * must replace an older positive-torque command that has not been sent. */
    if(xQueueOverwrite(dev->tx_queue, request) != pdPASS)
    {
        dev->tx_dropped_count++;
        return HAL_BUSY;
    }

    return HAL_OK;
}

HAL_StatusTypeDef canbus_wait_tx_ready(canbus_t *dev, uint32_t timeout_ms)
{
    uint32_t start;

    if((dev == NULL) || (dev->hcan == NULL))
    {
        return HAL_ERROR;
    }

    /* HAL_GetTick() is intentionally used only as a coarse upper bound here.
     * Entry relative to the 1 ms tick means an N-ms request can expire almost
     * one tick early (for example, nominal 2 ms behaves approximately as
     * [1 ms, 2 ms)). That bias is fail-zero for CM200 torque commit. Do not
     * "tighten" this to 1 ms without target DWT evidence or the effective
     * interval becomes approximately [0 ms, 1 ms). The caller records DWT
     * wait duration so hardware tests quantify the actual duty/jitter. */
    start = HAL_GetTick();
    for(;;)
    {
        uint32_t pending;
        bool uncertain;
        taskENTER_CRITICAL();
        pending = dev->tx_pending_mailbox_mask;
        uncertain = dev->tx_outcome_uncertain_latched;
        taskEXIT_CRITICAL();

        if(uncertain)
        {
            return HAL_ERROR;
        }
        if((pending == 0u) &&
           (HAL_CAN_GetTxMailboxesFreeLevel(dev->hcan) != 0u))
        {
            return HAL_OK;
        }
        if((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
        {
            return HAL_TIMEOUT;
        }
        taskYIELD();
    }
}

HAL_StatusTypeDef canbus_transmit_ready(canbus_t *dev,
                                        const canbus_packet_t *packet)
{
    return canbus_transmit_ready_tracked(dev, packet, NULL);
}

HAL_StatusTypeDef canbus_transmit_ready_tracked(canbus_t *dev,
                                                const canbus_packet_t *packet,
                                                uint32_t *mailbox)
{
    uint8_t payload[DATALEN];

    if((dev == NULL) || (packet == NULL) || (dev->hcan == NULL) ||
       (dev->tx_header == NULL))
    {
        return HAL_ERROR;
    }

    /* The caller must have established that a mailbox is free.  Keeping the
     * non-blocking hardware enqueue separate from the wait lets the CM200 task
     * perform its final AMS authority re-read after any mailbox delay and then
     * commit the command while CAN RX is masked. */
    if(HAL_CAN_GetTxMailboxesFreeLevel(dev->hcan) == 0u)
    {
        return HAL_BUSY;
    }

    HAL_StatusTypeDef status;
    uint32_t selected_mailbox = 0u;
    taskENTER_CRITICAL();

    /* Never enqueue behind an unresolved software-owned mailbox.  bxCAN can
     * complete a mailbox in hardware while its completion IRQ is masked by
     * this critical section.  If we called HAL_CAN_AddTxMessage() with an old
     * software token still live, HAL could immediately reuse that newly-free
     * mailbox and assert TXRQ before software noticed the ownership collision.
     * An abort after that enqueue is too late to prove the new frame never hit
     * the bus.  Serialize hardware enqueue until the prior callback/error path
     * has reconciled ownership.  This deliberately trades mailbox parallelism
     * for deterministic command identity. */
    if(dev->tx_outcome_uncertain_latched)
    {
        taskEXIT_CRITICAL();
        return HAL_ERROR;
    }
    if(dev->tx_pending_mailbox_mask != 0u)
    {
        taskEXIT_CRITICAL();
        return HAL_BUSY;
    }

    dev->tx_header->StdId = packet->id;
    dev->tx_header->DLC = DATALEN;
    memcpy(payload, packet->data, sizeof(payload));
    status = HAL_CAN_AddTxMessage(dev->hcan, dev->tx_header, payload,
                                  &selected_mailbox);
    if(status == HAL_OK)
    {
        /* selected_mailbox must be a real bxCAN mailbox token.  Software
         * ownership was proven empty before HAL asserted TXRQ above. */
        if(selected_mailbox == 0u)
        {
            status = HAL_ERROR;
        }
        else
        {
            dev->tx_pending_mailbox_mask |= selected_mailbox;
            dev->tx_complete_mailbox_mask &= ~selected_mailbox;
            dev->tx_abort_mailbox_mask &= ~selected_mailbox;
            dev->tx_mailbox = selected_mailbox;
            if(mailbox != NULL) *mailbox = selected_mailbox;
        }
    }
    taskEXIT_CRITICAL();
    return status;
}

HAL_StatusTypeDef canbus_wait_tx_complete(canbus_t *dev, uint32_t mailbox,
                                          uint32_t timeout_ms)
{
    if((dev == NULL) || (dev->hcan == NULL) || (mailbox == 0u))
    {
        return HAL_ERROR;
    }

    const uint32_t start = HAL_GetTick();
    for(;;)
    {
        HAL_StatusTypeDef result = HAL_BUSY;
        taskENTER_CRITICAL();
        if((dev->tx_complete_mailbox_mask & mailbox) != 0u)
        {
            dev->tx_complete_mailbox_mask &= ~mailbox;
            result = HAL_OK;
        }
        else if((dev->tx_abort_mailbox_mask & mailbox) != 0u)
        {
            dev->tx_abort_mailbox_mask &= ~mailbox;
            result = HAL_ERROR;
        }
        taskEXIT_CRITICAL();
        if(result != HAL_BUSY) return result;

        if((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
        {
            taskENTER_CRITICAL();
            if(dev->tx_complete_timeout_count != UINT32_MAX)
            {
                dev->tx_complete_timeout_count++;
            }
            taskEXIT_CRITICAL();

            /* The deadline expired, but the physical outcome can still race
             * the abort request.  Do not immediately discard that outcome:
             * a TXOK just after the deadline means the inverter consumed this
             * rolling counter and software must commit it.  Request abort,
             * then allow a short bounded window for the actual bxCAN terminal
             * callback to reconcile ownership. */
            (void)HAL_CAN_AbortTxRequest(dev->hcan, mailbox);
            const uint32_t reconcile_start = HAL_GetTick();
            for(;;)
            {
                HAL_StatusTypeDef reconcile = HAL_BUSY;
                taskENTER_CRITICAL();
                if((dev->tx_complete_mailbox_mask & mailbox) != 0u)
                {
                    dev->tx_complete_mailbox_mask &= ~mailbox;
                    reconcile = HAL_OK;
                }
                else if((dev->tx_abort_mailbox_mask & mailbox) != 0u)
                {
                    dev->tx_abort_mailbox_mask &= ~mailbox;
                    reconcile = HAL_ERROR;
                }
                taskEXIT_CRITICAL();

                if(reconcile != HAL_BUSY)
                {
                    return reconcile;
                }
                if((uint32_t)(HAL_GetTick() - reconcile_start) >=
                   CANBUS_TX_ABORT_RECONCILE_MS)
                {
                    /* We can no longer prove whether this command reached the
                     * inverter.  A callback may still arrive after this
                     * function returns; clearing only the mailbox token then
                     * would be unsafe because the caller has already chosen
                     * not to advance the CM200 rolling counter.  Latch the
                     * uncertainty and prohibit every future enqueue until a
                     * deliberate CAN/device reinitialization resets state.
                     * This sacrifices liveness, but never allows a second
                     * ambiguously-countered command onto the bus. */
                    taskENTER_CRITICAL();
                    dev->tx_outcome_uncertain_latched = true;
                    taskEXIT_CRITICAL();
                    return HAL_TIMEOUT;
                }
                osDelay(1u);
            }
        }
        osDelay(1u);
    }
}

static void canbus_tx_callback_isr(canbus_t *dev, uint32_t mailbox,
                                   bool completed)
{
    if((dev == NULL) || (mailbox == 0u)) return;
    if((dev->tx_pending_mailbox_mask & mailbox) == 0u)
    {
        if(dev->tx_unexpected_callback_count != UINT32_MAX)
        {
            dev->tx_unexpected_callback_count++;
        }
        return;
    }
    dev->tx_pending_mailbox_mask &= ~mailbox;
    if(completed)
    {
        dev->tx_complete_mailbox_mask |= mailbox;
        if(dev->tx_complete_count != UINT32_MAX) dev->tx_complete_count++;
    }
    else
    {
        dev->tx_abort_mailbox_mask |= mailbox;
        if(dev->tx_abort_count != UINT32_MAX) dev->tx_abort_count++;
    }
}

void canbus_tx_complete_isr(canbus_t *dev, uint32_t mailbox)
{
    canbus_tx_callback_isr(dev, mailbox, true);
}

void canbus_tx_abort_isr(canbus_t *dev, uint32_t mailbox)
{
    canbus_tx_callback_isr(dev, mailbox, false);
}

void canbus_tx_error_isr(canbus_t *dev, uint32_t error_code)
{
    if(dev == NULL) return;

    /* HAL reports mailbox-specific terminal TX errors after it has consumed
     * the hardware request-complete flag. Retire only those exact tokens. RX
     * FIFO overflow, warning/passive state and ordinary LEC errors do not alter
     * TX ownership and may coexist with a valid pending transmission. */
    uint32_t failed = 0u;
    if((error_code & (HAL_CAN_ERROR_TX_ALST0 | HAL_CAN_ERROR_TX_TERR0)) != 0u)
        failed |= CAN_TX_MAILBOX0;
    if((error_code & (HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_TX_TERR1)) != 0u)
        failed |= CAN_TX_MAILBOX1;
    if((error_code & (HAL_CAN_ERROR_TX_ALST2 | HAL_CAN_ERROR_TX_TERR2)) != 0u)
        failed |= CAN_TX_MAILBOX2;

    if((failed & CAN_TX_MAILBOX0) != 0u)
        canbus_tx_abort_isr(dev, CAN_TX_MAILBOX0);
    if((failed & CAN_TX_MAILBOX1) != 0u)
        canbus_tx_abort_isr(dev, CAN_TX_MAILBOX1);
    if((failed & CAN_TX_MAILBOX2) != 0u)
        canbus_tx_abort_isr(dev, CAN_TX_MAILBOX2);

    /* Controller-level failures request abort of what is still pending. Keep
     * software ownership until the mailbox abort/complete callback reports the
     * real hardware outcome. */
    const uint32_t fatal = HAL_CAN_ERROR_BOF | HAL_CAN_ERROR_TIMEOUT |
                           HAL_CAN_ERROR_NOT_INITIALIZED |
                           HAL_CAN_ERROR_NOT_READY |
                           HAL_CAN_ERROR_NOT_STARTED |
                           HAL_CAN_ERROR_INTERNAL;
    if(((error_code & fatal) != 0u) && (dev->hcan != NULL))
    {
        const uint32_t pending = dev->tx_pending_mailbox_mask;
        if(pending != 0u)
        {
            (void)HAL_CAN_AbortTxRequest(dev->hcan, pending);
        }
    }
}

HAL_StatusTypeDef canbus_transmit(canbus_t *dev, const canbus_packet_t *packet,
                                  uint32_t timeout_ms)
{
    HAL_StatusTypeDef status = canbus_wait_tx_ready(dev, timeout_ms);
    if(status != HAL_OK)
    {
        return status;
    }
    return canbus_transmit_ready(dev, packet);
}
