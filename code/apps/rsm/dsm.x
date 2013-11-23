typedef uint32_t dsm_count;

program DSM_PROG {
    version DSM_VERS1 {
        void
        DSM_NULL(void) = 0;

        dsm_count 
        DSM_INC() = 1;

        dsm_count
        DSM_COUNT() = 2;
    } = 1;
} = 482633;
