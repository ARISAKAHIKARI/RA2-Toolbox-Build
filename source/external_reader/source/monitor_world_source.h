#pragma once
#include "process_connection.h"

namespace external {
// Assist owns the lease. A monitor running without Assist keeps its existing
// standalone reader. While a matching producer is connected, missing pages
// must stay missing until a new publication completes them.
inline std::shared_ptr<const ProcessConnection> monitorWorldProducer;
inline void setMonitorWorldProducer(std::shared_ptr<const ProcessConnection> connection){
 std::atomic_store(&monitorWorldProducer,std::move(connection));
}
inline bool hasMonitorWorldProducer(uint32_t pid,uint64_t generation){
 const auto connection=std::atomic_load(&monitorWorldProducer);
 return connection&&connection->pid==pid&&connection->generation==generation;
}
}
