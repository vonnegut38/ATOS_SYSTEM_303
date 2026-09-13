/**
 * Host half of the Polish language pack.
 *
 * The whole contribution lives in the browser half (`./client`): the language
 * definition and the dictionaries are registered on the client `locale`
 * service. This half exists so the package can be mounted as an ordinary
 * Loader row, which is what makes the host compose and serve the client
 * bundle. It owns no Host state and registers nothing model-facing.
 */

/** Host half: no Host-side configuration to register. */
export function apply() {}
