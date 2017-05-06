/* The MIT License
 * 
 * Copyright (c) 2017 John Schember <john@nachtimwald.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 */

#include <string.h>

#include "str_helpers.h"
#include "xml_helpers.h"

/* XPath parsing helper. Takes all nodes matching an xpath and runs them
 * through a callback function for further processing. */
void parse_nodes_int(const char *xml, const char *xpath, node_processor_cb_t np, void *arg)
{
    xmlDocPtr           doc;
    xmlXPathContextPtr  xctx;
    xmlXPathObjectPtr   xobj;
    xmlNodePtr          cur;
    size_t              len;
    size_t              i;

    if (str_isempty(xml) || str_isempty(xpath) || np == NULL)
        return;

    doc = xmlParseMemory(xml, strlen(xml));
    if (doc == NULL)
        return;

    xctx = xmlXPathNewContext(doc);
    if (xctx == NULL) {
        xmlFreeDoc(doc);
        return;
    }

    xobj = xmlXPathEvalExpression((const xmlChar *)xpath, xctx);
    if (xobj == NULL) {
        xmlXPathFreeContext(xctx);
        xmlFreeDoc(doc);
        return;
    }

    if (xobj->nodesetval == NULL) {
        xmlXPathFreeObject(xobj);
        xmlXPathFreeContext(xctx);
        xmlFreeDoc(doc);
        return;
    }

    len = xobj->nodesetval->nodeNr;
    for (i=0; i<len; i++) {
        cur = xobj->nodesetval->nodeTab[i];
        if (!np(doc, cur, arg))
            break;
    }

    xmlXPathFreeObject(xobj);
    xmlXPathFreeContext(xctx);
    xmlFreeDoc(doc);
}

/* Get the text from the first node matching the XPath. This should only be
 * used when there is known to be only one node. If there are multiple nodes
 * either because it's a repeating tag or because the XPath encompasses
 * matching multiple tags, then use parse_nodes_int. */
char *get_xml_text(const char *xpath, xmlDocPtr doc, xmlNodePtr node)
{
    char               *text;
    xmlChar            *xtext;
    xmlXPathContextPtr  xctx;
    xmlXPathObjectPtr   xobj;
    xmlNodePtr          cur;

    xctx = xmlXPathNewContext(doc);
    if (xctx == NULL)
        return NULL;

    if (node != NULL) {
        if (xmlXPathSetContextNode(node, xctx) != 0) {
            xmlXPathFreeContext(xctx);
            return NULL;
        }
    }

    xobj = xmlXPathEvalExpression((const xmlChar *)xpath, xctx);
    if (xobj == NULL) {
        xmlXPathFreeContext(xctx);
        return NULL;
    }

    if (xobj->nodesetval == NULL) {
        xmlXPathFreeObject(xobj);
        xmlXPathFreeContext(xctx);
        return NULL;
    }

    if (xobj->nodesetval->nodeNr == 0) {
        xmlXPathFreeObject(xobj);
        xmlXPathFreeContext(xctx);
        return NULL;
    }

    cur   = xobj->nodesetval->nodeTab[0];
    xtext = xmlNodeGetContent(cur);
    text  = strdup((const char *)xtext);
    xmlFree(xtext);

    xmlXPathFreeObject(xobj);
    xmlXPathFreeContext(xctx);
    return text;
}
