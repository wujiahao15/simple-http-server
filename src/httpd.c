#include "httpd.h"

int main(int argc, char* argv[]) {
    // return value of main function
    int ret = 0;
    struct event* term = NULL;
    
    // parse options from command line
    struct options opt = parse_opts(argc, argv);

    // create event config
    struct event_config* cfg = event_config_new();
    struct event_base* base = event_base_new_with_config(cfg);
    if (!base) {
        logger(ERROR, "Couldn't create an event_base: exiting\n");
        ret = 1;
    }
    // free event config when finished
    event_config_free(cfg);
    cfg = NULL;

    // create a new evhttp object to handle requests.
    struct evhttp* http = evhttp_new(base);
    if (!http) {
        logger(ERROR, "couldn't create evhttp. Exiting.\n");
        ret = 1;
    }

    /* The /dump URI will dump all requests to stdout and say 200 ok. */
    evhttp_set_cb(http, "/", handle_request_cb, NULL);

    /* We want to accept arbitrary requests, so we need to set a "generic"
     * cb.  We can also add callbacks for specific paths. */
    evhttp_set_gencb(http, send_document_cb, &opt);

    // bind socket to http server
    struct evhttp_bound_socket* handle =
        evhttp_bind_socket_with_handle(http, "0.0.0.0", opt.port);
    if (!handle) {
        logger(ERROR, "couldn't bind to port %d. Exiting.\n", opt.port);
        ret = 1;
        goto err;
    }

    if (display_listen_sock(handle)) {
        ret = 1;
        goto err;
    }

    // add signal handler to event base
    term = evsignal_new(base, SIGINT, do_term, base);
    if (!term)
        goto err;
    if (event_add(term, NULL))
        goto err;

    // dispatch event
    event_base_dispatch(base);

    // error handling
err:
    if (cfg)
        event_config_free(cfg);
    if (http)
        evhttp_free(http);
    if (term)
        event_free(term);
    if (base)
        event_base_free(base);
    return ret;
}
