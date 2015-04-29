/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#include "mitkRdfStore.h"

#include <iostream>
#include <mitkCommon.h>

namespace mitk {
  class RdfStorePrivate {
  public:
    RdfStorePrivate();
    ~RdfStorePrivate();
  };

  RdfStore::RdfStore()
    : m_World(0), m_Storage(0), m_Model(0)
  {
    // SetUp base prefixes
    m_Prefixes["rdf"] = RdfUri("http://www.w3.org/1999/02/22-rdf-syntax-ns#");
    m_Prefixes["rdfs"] = RdfUri("http://www.w3.org/2000/01/rdf-schema#");
    m_Prefixes["xsd"] = RdfUri("http://www.w3.org/2001/XMLSchema#");
    m_Prefixes["owl"] = RdfUri("http://www.w3.org/2002/07/owl#");

    // SetUp new store
    CleanUp();
  }

  RdfStore::~RdfStore()
  {
    if(m_Model)
      librdf_free_model(m_Model);
    if(m_Storage)
      librdf_free_storage(m_Storage);
    if(m_World)
      librdf_free_world(m_World);
  }

  void RdfStore::SetBaseUri(RdfUri uri)
  {
    m_BaseUri = uri;
    m_Prefixes[""] = RdfUri(m_BaseUri.ToString());
  }

  RdfUri RdfStore::GetBaseUri() const
  {
    return m_BaseUri;
  }

  void RdfStore::AddPrefix(std::string prefix, RdfUri uri)
  {
    m_Prefixes[prefix] = uri;
  }

  RdfStore::PrefixMap RdfStore::GetPrefixes() const
  {
    return m_Prefixes;
  }

  void RdfStore::CleanUp()
  {
    // CleanUp old Store if there is one
    if(m_Model)
      librdf_free_model(m_Model);
    if(m_Storage)
      librdf_free_storage(m_Storage);
    if(m_World)
      librdf_free_world(m_World);

    // SetUp new Store
    m_World = librdf_new_world();
    librdf_world_open(m_World);

    m_Storage = librdf_new_storage(m_World, "memory", 0, 0);

    if(!m_Storage)
    {
      mitkThrow() << "RDF Library Error";
    }

    m_Model = librdf_new_model(m_World, m_Storage, 0);

    if(!m_Model)
    {
      mitkThrow() << "RDF Library Error";
    }
  }

  bool RdfStore::Add(RdfTriple triple)
  {
    librdf_statement* statement = RdfTripleToStatement(triple);

    if (!CheckComplete(statement))
    {
      librdf_free_statement(statement);
      return false;
    }

    // Store already contains statement
    if (Contains(triple)) return true;

    if (librdf_model_add_statement(m_Model, statement) != 0) {
      librdf_free_statement(statement);
      return false;
    }
    else
    {
      librdf_free_statement(statement);
      return true;
    }
  }

  bool RdfStore::Remove(RdfTriple triple)
  {
    librdf_statement* statement = RdfTripleToStatement(triple);

    if (!CheckComplete(statement))
    {
      librdf_free_statement(statement);
      return false;
    }

    // Store does not contain statement
    if (!Contains(triple)) return true;

    if (librdf_model_remove_statement(m_Model, statement) != 0) {
      librdf_free_statement(statement);
      return false;
    }
    else
    {
      librdf_free_statement(statement);
      return true;
    }
    return false;
  }

  bool RdfStore::Contains(RdfTriple triple)
  {
    librdf_statement* statement = RdfTripleToStatement(triple);

    // if 0 there is no triple
    if (librdf_model_contains_statement(m_Model, statement) == 0) {
      librdf_free_statement(statement);
      return false;
    }
    else
    {
      librdf_free_statement(statement);
      return true;
    }
    return false;
  }

  RdfStore::ResultMap RdfStore::Query(std::string sparqlQuery) const
  {
    RdfStore::ResultMap resultMap;
    std::string completeQuery;

    for (PrefixMap::const_iterator i = m_Prefixes.begin(); i != m_Prefixes.end(); i++)
    {
      completeQuery += "PREFIX " + i->first + ": " + "<" + i->second.ToString() + "> ";
    }
    completeQuery += sparqlQuery;

    librdf_query* rdfQuery = librdf_new_query(m_World, "sparql", 0, (const unsigned char*) completeQuery.c_str(), 0);

    if (!rdfQuery) return resultMap;

    librdf_query_results* results = librdf_query_execute(rdfQuery, m_Model);

    if (!results)
    {
      librdf_free_query(rdfQuery);
      return resultMap;
    }

    if (!librdf_query_results_is_bindings(results))
    {
      librdf_free_query_results(results);
      librdf_free_query(rdfQuery);
      return resultMap;
    }

    while (!librdf_query_results_finished(results))
    {
      int count = librdf_query_results_get_bindings_count(results);

      for (int i = 0; i < count; ++i)
      {
        const char *name = librdf_query_results_get_binding_name(results, i);

        if (!name) continue;

        std::string key = name;

        std::list<RdfNode> list;

        if(!resultMap[key].empty())
        {
          list = resultMap[key];
        }

        librdf_node *node = librdf_query_results_get_binding_value(results, i);

        list.push_front(LibRdfNodeToRdfNode(node));

        resultMap[key] = list;
      }

      librdf_query_results_next(results);
    }

    librdf_free_query_results(results);
    librdf_free_query(rdfQuery);

    return resultMap;
  }

  void RdfStore::Save(std::string filename, std::string format)
  {
    if (format == "") format = "turtle";

    librdf_uri* baseUri = RdfUriToLibRdfUri(m_BaseUri);

    librdf_serializer* s = librdf_new_serializer(m_World, format.c_str(), 0, 0);

    if(!s)
    {
      mitkThrow() << "RDF Library Error";
    }

    for (PrefixMap::const_iterator i = m_Prefixes.begin(); i != m_Prefixes.end(); i++)
    {
      librdf_serializer_set_namespace(s, RdfUriToLibRdfUri(i->second), i->first.c_str());
    }

    FILE* f = fopen(filename.c_str(), "w+");

    librdf_serializer_serialize_model_to_file_handle(s, f, baseUri, m_Model);

    librdf_free_serializer(s);
    librdf_free_uri(baseUri);
    fclose(f);
  }

  void RdfStore::Import(std::string url, std::string format)
  {
    std::string baseUri = m_BaseUri.ToString();

    if (baseUri.empty())
    {
      baseUri = url;
      SetBaseUri(RdfUri(baseUri));
    }

    if (format == "") format= "turtle";

    // Redland uses file paths like file:YOURPATH ( Example: file:D:/home/readme.txt )
    librdf_uri* uri = librdf_new_uri(m_World, (const unsigned char*) url.c_str());

    librdf_uri* libRdfBaseUri = librdf_new_uri(m_World, (const unsigned char*) baseUri.c_str());

    librdf_parser* p = librdf_new_parser(m_World, format.c_str(), 0, 0);

    if(!p)
    {
      mitkThrow() << "RDF Library Error";
    }

    if (librdf_parser_parse_into_model(p, uri, libRdfBaseUri, m_Model) != 0 )
    {
      librdf_free_parser(p);
      MITK_ERROR << "Parsing into Model failed.";
      return;
    }

    int namespaces = librdf_parser_get_namespaces_seen_count(p);

    for (int i = 0; i < namespaces; i++) {
      const char* prefixChar = librdf_parser_get_namespaces_seen_prefix(p, i);

      if ( !prefixChar ) continue;

      std::string prefix = prefixChar;
      RdfUri uri = LibRdfUriToRdfUri(librdf_parser_get_namespaces_seen_uri(p, i));

      if (uri == RdfUri()) return;

      RdfStore::PrefixMap::iterator it = m_Prefixes.find(prefix);

      // map iterator is equal to iterator-end so it is not already added
      if (it == m_Prefixes.end()) {
        AddPrefix(prefix, uri);
      }
    }
    librdf_free_parser(p);
  }

  /*****************************************************************************
  ********************************** Private **********************************
  *****************************************************************************/

  bool RdfStore::CheckComplete(librdf_statement* statement)
  {
    if (librdf_statement_is_complete(statement) != 0) return true;
    else return false;
  }

  librdf_statement* RdfStore::RdfTripleToStatement(RdfTriple triple) const
  {
    librdf_node* subject = RdfNodeToLibRdfNode(triple.GetTripleSubject());
    librdf_node* predicate = RdfNodeToLibRdfNode(triple.GetTriplePredicate());
    librdf_node* object = RdfNodeToLibRdfNode(triple.GetTripleObject());

    librdf_statement* statement = librdf_new_statement_from_nodes(m_World, subject, predicate, object);
    if(!statement) return 0;
    return statement;
  }

  librdf_node* RdfStore::RdfNodeToLibRdfNode(RdfNode node) const
  {
    librdf_node* newNode = 0;

    switch (node.GetType())
    {
    case RdfNode::NOTHING:
      break;
    case RdfNode::BLANK:
      newNode = librdf_new_node_from_blank_identifier(m_World, (const unsigned char*) node.GetValue().c_str());
      break;
    case RdfNode::LITERAL:
      {
        if (node.GetDatatype() != RdfUri())
        {
          librdf_uri* typeUri = RdfUriToLibRdfUri(node.GetDatatype());
          newNode = librdf_new_node_from_typed_literal(m_World, (const unsigned char*) node.GetValue().c_str(), 0, typeUri);
        }
        else
        {
          newNode = librdf_new_node_from_literal(m_World, (const unsigned char*) node.GetValue().c_str(), 0, 0);
        }
      }
      break;
    case RdfNode::URI:
      newNode = librdf_new_node_from_uri( m_World, librdf_new_uri(m_World, (const unsigned char*) node.GetValue().c_str()) );
      break;
    default:
      break;
    }
    return newNode;
  }

  librdf_uri* RdfStore::RdfUriToLibRdfUri(RdfUri uri) const
  {
    librdf_uri* libUri = librdf_new_uri(m_World, (const unsigned char*) uri.ToString().c_str());
    if (!libUri) return 0;
    return libUri;
  }

  RdfTriple RdfStore::StatementToRdfTriple(librdf_statement* statement) const
  {
    librdf_node *subject = librdf_statement_get_subject(statement);
    librdf_node *predicate = librdf_statement_get_predicate(statement);
    librdf_node *object = librdf_statement_get_object(statement);

    RdfTriple triple(LibRdfNodeToRdfNode(subject),
      LibRdfNodeToRdfNode(predicate),
      LibRdfNodeToRdfNode(object));

    return triple;
  }

  RdfNode RdfStore::LibRdfNodeToRdfNode(librdf_node* node) const
  {
    RdfNode mitkNode;

    if (!node) return 0;

    if (librdf_node_is_resource(node))
    {
      mitkNode.SetType(RdfNode::URI);
      librdf_uri *uri = librdf_node_get_uri(node);
      mitkNode.SetValue(LibRdfUriToRdfUri(uri).ToString());
    }
    else if (librdf_node_is_literal(node))
    {
      mitkNode.SetType(RdfNode::LITERAL);
      std::string value = (const char*) librdf_node_get_literal_value(node);
      if (!value.empty()) mitkNode.SetValue(value);
      librdf_uri* typeUri = librdf_node_get_literal_value_datatype_uri(node);
      if (typeUri) mitkNode.SetDatatype(LibRdfUriToRdfUri(typeUri));
    }
    else if (librdf_node_is_blank(node))
    {
      mitkNode.SetType(RdfNode::BLANK);
      std::string str = (const char*) librdf_node_get_blank_identifier(node);
      if (!str.empty()) mitkNode.SetValue(str);
    }
    return mitkNode;
  }

  RdfUri RdfStore::LibRdfUriToRdfUri(librdf_uri* uri) const
  {
    std::string str = (const char*) librdf_uri_as_string(uri);
    if (!str.empty()) return RdfUri(str);

    return RdfUri();
  }
} // end of namespace mitk
