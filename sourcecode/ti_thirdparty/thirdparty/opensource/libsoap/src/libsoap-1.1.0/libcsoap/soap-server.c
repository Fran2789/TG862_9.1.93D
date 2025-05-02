/******************************************************************
 *  $Id: soap-server.c,v 1.26 2006/07/09 16:24:19 snowdrop Exp $
 *
 * CSOAP Project:  A SOAP client/server library in C
 * Copyright (C) 2003  Ferhat Ayaz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 * 
 * Email: ayaz@jprogrammer.net
 ******************************************************************/ 
 /* Modifications Copyright (c) 2017 ARRIS Enterprises, LLC. */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef WIN32
#define snprintf(buffer, num, s1, s2) sprintf(buffer, s1,s2)
#endif

#include <nanohttp/nanohttp-logging.h>
#include <nanohttp/nanohttp-server.h>

#include "soap-admin.h"
#include "soap-server.h"
#include "autoconf.h"

static SoapRouterNode *head = NULL;
static SoapRouterNode *tail = NULL;

// static SoapRouter *router_find(const char *context);

    static void
_soap_server_send_env(http_output_stream_t * out, SoapEnv * env)
{
    xmlBufferPtr buffer;
    if (env == NULL || env->root == NULL)
        return;

    buffer = xmlBufferCreate();
    xmlNodeDump(buffer, env->root->doc, env->root, 1, 1);
    http_output_stream_write_string(out,
            (const char *) xmlBufferContent(buffer));
    xmlBufferFree(buffer);

    return;
}

//ARRIS ADD START
    static void
_soap_server_send_authfail(httpd_conn_t * conn, const char *errmsg)
{
      const char * template1 =
        "<html>"
        "<head>"
        "<title>Unauthorized</title>"
        "</head>"
        "<body>"
        "<h1>Unauthorized request logged</h1>" "</body>" "</html>" "\r\n";
      char buflen[5];

      snprintf(buflen, 5, "%d", strlen(template1));
      
      httpd_set_header(conn, HEADER_CONTENT_LENGTH, buflen);
      httpd_set_header(conn, HEADER_CONTENT_TYPE, "text/xml");
      httpd_set_header(conn, HEADER_WWW_AUTHENTICATE, "Basic realm=\"nanoHTTP\"");
      
      httpd_send_header(conn, 401, "Unauthorized");
      http_output_stream_write_string(conn->out, template1);

    return;        
}
//ARRIS ADD END

//ARRIS MOD START
    static void
_soap_server_send_fault(httpd_conn_t * conn, const char *errmsg)
{
    SoapEnv *envres;
    herror_t err;

    err = soap_env_new_with_fault(Fault_Server, errmsg ? errmsg : "General error", "cSOAP_Server", NULL, &envres);
    if (err != H_OK)
    {
        const char * template1 = "<html><head></head><body>"
                                     "<h1>Error</h1><hr/>"
                                     "Error while sending fault object:<br />Message: %s"
                                     "<br />Function: %s"
                                     "<br />Error code: %d"
                                     "</body></html>\r\n";
        char buffer[4064];
        char buflen[5];

        log_error1(herror_message(err));

        snprintf(buffer, 4064, template1, herror_message(err), herror_func(err), herror_code(err));
        snprintf(buflen, 5, "%d", strlen(buffer));

        httpd_set_header(conn, HEADER_CONTENT_LENGTH, buflen);
        httpd_set_header(conn, HEADER_CONTENT_TYPE, "text/xml");

        //send message head
        httpd_send_header(conn, 500, "FAILED");
        //send message body
        http_output_stream_write_string(conn->out, buffer);

        herror_release(err);
    }
    else
    {
        xmlBufferPtr buffer;
        char buflen[5];

        buffer = xmlBufferCreate();
        xmlNodeDump(buffer, envres->root->doc, envres->root, 1, 1);

        snprintf(buflen, 5, "%d", xmlBufferLength(buffer));

        httpd_set_header(conn, HEADER_CONTENT_LENGTH, buflen);
        httpd_set_header(conn, HEADER_CONTENT_TYPE, "text/xml");

        //send message head
        httpd_send_header(conn, 500, "FAILED");
        //send message body
        http_output_stream_write_string(conn->out, (const char *) xmlBufferContent(buffer));
        
        xmlBufferFree(buffer);
    }

    return;
}
//ARRIS MOD END

    static void
_soap_server_send_ctx(httpd_conn_t * conn, SoapCtx * ctx)
{
    static int counter = 1;
#if 0	
    xmlBufferPtr buffer;
#endif
    char strbuffer[32];
    part_t *part;
/* SERCOMM ADD	*/
    xmlChar *buf=NULL;
	int buf_len;
/* SERCOMM ADD	END */

    if (ctx->env == NULL || ctx->env->root == NULL || ctx->env->root->doc == NULL)
        return;

    xmlThrDefIndentTreeOutput(1);
    /*  xmlKeepBlanksDefault(0);*/
#if 0 //PROD00208849, remove original code
    buffer = xmlBufferCreate();
    xmlNodeDump(buffer, ctx->env->root->doc, ctx->env->root, 1, 1);
#endif
    /* SERCOMM ADD  */
    /* PROD00208849
            Change dump function, xmlNodeDump only dump nodes thus no xml declaration.
            Use xmlDocDumpMemory to dump whole xml file to http stream.*/
    xmlDocDumpMemory(ctx->env->root->doc, &buf, &buf_len);
    /* SERCOMM ADD  END */

    if (ctx->attachments)
    {
        sprintf(strbuffer, "000128590350940924234%d", counter++);
        httpd_mime_send_header(conn, strbuffer, "", "text/xml", 200, "OK");
        httpd_mime_next(conn, strbuffer, "text/xml", "binary");
#if 0
        http_output_stream_write_string(conn->out,
                (const char *) xmlBufferContent(buffer));
#endif
        /* SERCOMM ADD  */
        http_output_stream_write_string(conn->out, (const char *) buf);
        /* SERCOMM ADD  END */

        part = ctx->attachments->parts;
        while (part)
        {
            httpd_mime_send_file(conn, part->id, part->content_type,
                    part->transfer_encoding, part->filename);
            part = part->next;
        }
        httpd_mime_end(conn);
    }
    else
    {
        char buflen[100];
        xmlXPathContextPtr xpathCtx;
        xmlXPathObjectPtr xpathObj;

        xpathCtx = xmlXPathNewContext(ctx->env->root->doc);
        xpathObj = xmlXPathEvalExpression("//Fault", xpathCtx);
#if 0
        snprintf(buflen, 100, "%d", xmlBufferLength(buffer));
#endif
        /* SERCOMM ADD  */
        snprintf(buflen, 100, "%d", buf_len);
        /* SERCOMM ADD  END */

        httpd_set_header(conn, HEADER_CONTENT_LENGTH, buflen);
        httpd_set_header(conn, HEADER_CONTENT_TYPE, "text/xml");

        if ((xpathObj->nodesetval) ? xpathObj->nodesetval->nodeNr : 0)
        {
            httpd_send_header(conn, 500, "FAILED");
        }
        else
        {
            httpd_send_header(conn, 200, "OK");
        }
#if 0
        http_output_stream_write_string(conn->out,
                (const char *) xmlBufferContent(buffer));
#endif
        /* SERCOMM ADD  */
		http_output_stream_write_string(conn->out,
		    			(const char *) buf);
        /* SERCOMM ADD  END */

        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);

    }
	/* SERCOMM ADD  */
	xmlFree(buf);
	/* SERCOMM ADD  END */
    return;
}

    static void
_soap_server_send_description(httpd_conn_t *conn, xmlDocPtr wsdl)
{
    char length[16];
    xmlBufferPtr buf;

    buf = xmlBufferCreate();
    xmlNodeDump(buf, wsdl, xmlDocGetRootElement(wsdl), 0, 0);

    sprintf(length, "%d", xmlBufferLength(buf));
    httpd_set_header(conn, HEADER_CONTENT_TYPE, "text/xml");
    httpd_set_header(conn, HEADER_CONTENT_LENGTH, length);
    httpd_send_header(conn, 200, "OK");

    http_output_stream_write_string(conn->out, xmlBufferContent(buf));

    xmlBufferFree(buf);

    return;
}

    static SoapRouterNode *
router_node_new(SoapRouter * router, const char *context, SoapRouterNode * next)
{
    const char *noname = "/lost_found";
    SoapRouterNode *node;

    if (!(node = (SoapRouterNode *) malloc(sizeof(SoapRouterNode)))) {

        log_error2("malloc failed (%s)", strerror(errno));
        return NULL;
    }

    if (context)
    {
        node->context = strdup(context);
    }
    else
    {
        log_warn2("context is null. Using '%s'", noname);
        node->context = strdup(noname);
    }

    node->router = router;
    node->next = next;

    return node;
}

    SoapRouter *
soap_server_find_router(const char *context)
{
    SoapRouterNode *node;

    for (node = head; node; node = node->next)
    {
        if (!strcmp(node->context, context))
            return node->router;
    }

    return NULL;
}
#define HNAP_DEBUG_LOG "/var/log/hnap_debug.log"
static int soap_server_get_format_version(xmlDocPtr doc)
{
    int version = 0;
    xmlBufferPtr buffer;
    xmlNodePtr root;

    if (doc == NULL)
    {
        puts("xmlDocPtr is NULL!");
        return;
    }

    root = xmlDocGetRootElement(doc);
    if (root == NULL)
    {
        puts("Empty document!");
        return;
    }
    buffer = xmlBufferCreate();
    xmlNodeDump(buffer, doc, root, 1, 0);

    /* verify soap format */
    char *pch=NULL;
    pch = strstr((const char *) xmlBufferContent(buffer), "www.w3.org/2001/XMLSchema");
    if ( pch != NULL )
    {
        version = 2001;
    }
    pch = strstr((const char *) xmlBufferContent(buffer), "www.w3.org/1999/XMLSchema");
    if ( pch != NULL )
    {
        version = 1999;
    }
    return version;
}

    static void
soap_server_entry(httpd_conn_t * conn, hrequest_t * req)
{
    char buffer[1054];
    char *urn;
    char *method;
    SoapCtx *ctx, *ctxres;
    SoapRouter *router;
    SoapService *service;
    SoapEnv *env;
    herror_t err;

    if (req->method == HTTP_REQUEST_GET)
    {
        char * hwModel = httpd_get_hwModel(); // ARRIS ADD
        printf("GET\n");

        char xml_header[4096] = \
                                "<?xml version=\"1.0\" encoding=\"utf-8\"?>" \
                                " <soap:Envelope xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">" \
                                " <soap:Body> ";

        char xml_body[4096] = \
                              " <GetDeviceSettingsResponse xmlns=\""URN"\">" \
                              " <GetDeviceSettingsResult>OK</GetDeviceSettingsResult>" \
                              " <Type>GatewayWithWiFi</Type>";
        char devicename[128];
        sprintf(devicename, " <DeviceName>%s</DeviceName>", hwModel);
        strcat(xml_body, devicename);
        char vendorname[128];
        sprintf(vendorname, " <VendorName>%s</VendorName>", "Arris Interactive, L.L.C.");
        strcat(xml_body, vendorname);
        char modeldescription[128];
        sprintf(modeldescription, " <ModelDescription>%s</ModelDescription>", hwModel);
        strcat(xml_body, modeldescription);
        char modelname[128];
        sprintf(modelname, " <ModelName>%s</ModelName>", hwModel);
        strcat(xml_body, modelname);
        char firmwareversion[128];
        sprintf(firmwareversion, " <FirmwareVersion>%s</FirmwareVersion>", CONFIG_VENDOR_ARRIS_FW_VERSION);
        strcat(xml_body, firmwareversion);
        char presentationurl[128] = " <PresentationURL>/</PresentationURL>";
        strcat(xml_body, presentationurl);

        //ARRIS MOD START
        char others[2048] = \
                            " <SOAPActions>" \
                            " <string>" URN "Reboot</string>" \
                            " <string>" URN "IsDeviceReady</string>" \
                            " <string>" URN "GetDeviceSettings</string>" \
                            " <string>" URN "GetRouterSettings</string>" \
                            " <string>" URN "GetWanSettings</string>" \
                            " <string>" URN "GetConnectedDevices</string>" \
                            " <string>" URN "GetPortMappings</string>" \
                            " <string>" URN "SetDeviceSettings</string>" \
                            " <string>" URN "SetRouterSettings</string>" \
                            " <string>" URN "AddPortMapping</string>" \
                            " <string>" URN "DeletePortMapping</string>" \
                            " <string>" URN "GetWLanRadios</string>" \
                            " <string>" URN "GetWLanRadioSettings</string>" \
                            " <string>" URN "GetWLanRadioSecurity</string>" \
                            " <string>" URN "SetWLanRadioSettings</string>" \
                            " <string>" URN "SetWLanRadioSecurity</string>" \
                            " <string>" URN "GetRouterLanSettings2</string>" \
                            " <string>" URN "SetRouterLanSettings2</string>" \
                            " <string>" URN "SetDeviceSettings2</string>" \
                            " </SOAPActions>" \
                            " </GetDeviceSettingsResponse>";

        char xml_end[48] = " </soap:Body> </soap:Envelope>";
        char xml[4096];
        char buflen[5];
        
        snprintf(xml, 4096, "%s%s%s%s", xml_header, xml_body, others, xml_end); //construct message body
        snprintf(buflen, 5, "%d", strlen(xml)); //get length of body
        
        httpd_set_header(conn, HEADER_CONTENT_LENGTH, buflen); //put the length to message head
        httpd_set_header(conn, HEADER_CONTENT_TYPE, "text/xml"); //put the type to message head
        
        httpd_send_header(conn, 200, "OK"); //send message head
        http_output_stream_write_string(conn->out, xml); //send message body
        //ARRIS MOD END
        
        return;
    }
    
    if (!(router = soap_server_find_router(req->path)))
    {
        _soap_server_send_fault(conn, "Cannot find router");
        return;
    }
    else if (req->method == HTTP_REQUEST_GET && router->wsdl)
    {
        _soap_server_send_description(conn, router->wsdl);
        return;
    }

    if (req->method != HTTP_REQUEST_POST)
    {
        //ARRIS MOD START
        const char * template1 = "<html>"
                                                "<head>"
                "</head>"
                "<body>"
                "<h1>Sorry!</h1>"
                "<hr />"
                "<div>I only speak with 'POST' method </div>"
                "</body>"
                                                "</html>" "\r\n";
        char buflen[5];

        snprintf(buflen, 5, "%d", strlen(template1));

        httpd_set_header(conn, HEADER_CONTENT_LENGTH, buflen);
        httpd_set_header(conn, HEADER_CONTENT_TYPE, "text/xml");
        
        httpd_send_header(conn, 200, "OK");
        http_output_stream_write_string(conn->out, template1);
        //ARRIS MOD END
        return;
    }

    if ((err = soap_env_new_from_stream(req->in, &env)) != H_OK)
    {
        _soap_server_send_fault(conn, herror_message(err));
        herror_release(err);
        return;
    }

    if (env == NULL)
    {
        _soap_server_send_fault(conn, "Can not receive POST data!");
    }
    //ARRIS ADD START
    /* Check the result of authorization before parsing the POST data */
    else if ( conn->authret != 1 ) //authorize failed
    {
        _soap_server_send_authfail(conn, "Unauthorized request logged!");
    }
    //ARRIS ADD END
    else
    {
        ctx = soap_ctx_new(env);
        ctx->action = hpairnode_get_ignore_case(req->header, "SoapAction");
        if (ctx->action)
            ctx->action = strdup(ctx->action);

        ctx->http = req;
        soap_ctx_add_files(ctx, req->attachments);

        if (ctx->env == NULL)
        {
            _soap_server_send_fault(conn, "Can not parse POST data!");
        }
        else
        {
#if 0
            /* UNIHAN ADD START */
            int version=0;
            version = soap_server_get_format_version(env->root->doc);
            if ( version == 0 )
            {
                _soap_server_send_fault(conn, "No SOAP format version found!");
                soap_ctx_free(ctx);
                return;
            }
            ctx->env->soap_format_version = version;
            FILE *fp=NULL;
            fp = fopen(HNAP_DEBUG_LOG, "w");
            if ( fp != NULL )
            {
                fprintf(fp, "soap format version : %d\n", version);
                fclose(fp);
            }
            /* UNIHAN ADD END */  
#endif
            /* soap_xml_doc_print(env->root->doc); */

            if (!(urn=soap_env_find_urn(ctx->env)))
            {
                _soap_server_send_fault(conn, "No URN found!");
                soap_ctx_free(ctx);
                return;
            }
            else
            {
                log_verbose2("urn: '%s'", urn);
            }

            if (!(method=soap_env_find_methodname(ctx->env)))
            {
                _soap_server_send_fault(conn, "No method found!");
                soap_ctx_free(ctx);
                return;
            }
            else
            {
                log_verbose2("method: '%s'", method);
            }

            service = soap_router_find_service(router, urn, method);

            if (service == NULL)
            {
                sprintf(buffer, "URN '%s' not found", urn);
                _soap_server_send_fault(conn, buffer);
                soap_ctx_free(ctx);
                return;
            }
            else
            {
                log_verbose2("func: %p", service->func);
                ctxres = soap_ctx_new(NULL);
                /* ===================================== */
                /* CALL SERVICE FUNCTION */
                /* ===================================== */
                if ((err = service->func(ctx, ctxres)) != H_OK)
                {
                    sprintf(buffer, "Service returned following error message: '%s'",
                            herror_message(err));
                    herror_release(err);
                    _soap_server_send_fault(conn, buffer);
                    soap_ctx_free(ctx);
                    return;
                }

                if (ctxres->env == NULL)
                {

                    sprintf(buffer, "Service '%s' returned no envelope", urn);
                    _soap_server_send_fault(conn, buffer);
                    soap_ctx_free(ctx);
                    return;
                }
                else
                {

                    /*         httpd_send_header(conn, 200, "OK");
                               _soap_server_send_env(conn->out, ctxres->env);
                               */
                    _soap_server_send_ctx(conn, ctxres);
                    /* free envctx */
                    soap_ctx_free(ctxres);
                }
            }
        }
        soap_ctx_free(ctx);
    }
}

    herror_t
soap_server_init_args(int argc, char *argv[])
{
    herror_t err;
    if ((err = httpd_init(argc, argv)) != H_OK)
        return err;
    return soap_admin_init_args(argc, argv);
}

    int
soap_server_register_router(SoapRouter * router, const char *context)
{

    if (!httpd_register_secure(context, soap_server_entry, router->auth))
    {
        return 0;
    }

    if (tail == NULL)
    {
        head = tail = router_node_new(router, context, NULL);
    }
    else
    {
        tail->next = router_node_new(router, context, NULL);
        tail = tail->next;
    }

    return 1;
}

    SoapRouterNode *
soap_server_get_routers(void)
{
    return head;
}

    herror_t
soap_server_run(void)
{
    return httpd_run();
}

    int
soap_server_get_port(void)
{
    return httpd_get_port();
}

    const char *
soap_server_get_protocol(void)
{
    return httpd_get_protocol();
}

    void
soap_server_destroy()
{
    SoapRouterNode *node = head;
    SoapRouterNode *tmp;

    while (node != NULL)
    {
        tmp = node->next;
        log_verbose2("soap_router_free(%p)", node->router);
        soap_router_free(node->router);
        free(node->context);
        free(node);
        node = tmp;
    }
    httpd_destroy();

    return;
}
