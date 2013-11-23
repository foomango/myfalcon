void
spin() {
    for(;;);
}

void
recurse() {
    delaycb(0, 0, wrap(recurse));
}

void
saboteur(const char* failure) {
    if (!strcmp("segfault", failure)) {
        int *x = NULL;
        *x = 11;
    } else if (!strcmp("badloop", failure)) {
        delaycb(0,0, wrap(spin));
        for (;;);
    } else if (!strcmp("livelock", failure)) {
        delaycb(0,0, wrap(recurse));
    }
}
