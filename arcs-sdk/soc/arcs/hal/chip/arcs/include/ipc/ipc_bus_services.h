/**
 ****************************************************************************************
 *
 * @file ipc_bus_services.h
 *
 * @brief System-wide registry of IPC Bus service IDs.
 *
 * Every service that runs on the IPC Bus takes one entry here. Keeping all
 * service IDs in a single enum makes them globally unique by construction and
 * easy to audit. Per-service message/event IDs are NOT defined here; they are
 * local to each service and belong with that service's own module.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */
#ifndef _IPC_BUS_SERVICES_H_
#define _IPC_BUS_SERVICES_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Global IPC Bus service identifiers.
 *
 * service_id 0 is reserved as invalid, so numbering starts at 1. Add new
 * services below; do not reuse a retired number within the same build.
 */
typedef enum
{
    IPC_SVC_DEMO  = 1,   /* example service, see examples/ipc_bus */
} ipc_bus_service_id_t;

#ifdef __cplusplus
}
#endif

#endif /* _IPC_BUS_SERVICES_H_ */
