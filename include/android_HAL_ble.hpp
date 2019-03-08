#pragma once

int BTSetup();


/**
 * Connect to bluetooth peripheral
 * @param timeout: timeout after this many seconds. Set negative for no timeout.
 * @return: Non-zero on failure, zero on success.
 * TODO: needs arguments for what to connect to (presently hardcoded internally)
 */ 
int BTConnect(int timeout=-1);

int BTShutdown();
