struct [raypayload] RayPayload
{
    uint Hit : write(caller, closesthit, miss) : read(caller);
    uint Invoked : write(caller, closesthit, miss) : read(caller);
};
