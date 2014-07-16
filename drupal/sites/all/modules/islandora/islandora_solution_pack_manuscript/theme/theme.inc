<?php

/**
 * @file
 * Theme hooks.
 */

/**
 * Implements hook_preprocess_theme().
 */
function islandora_manuscript_preprocess_islandora_manuscript_manuscript(array &$variables) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_manuscript', 'includes/utilities');
  $object = $variables['object'];
  $params = array(
    'object' => $object,
    'pages' => islandora_paged_content_get_pages($object),
    'page_progression' => islandora_paged_content_get_page_progression($object),
    'transform_object' => islandora_manuscript_get_manuscript_transform_object($object),
  );
  module_load_include('inc', 'islandora', 'includes/solution_packs');
  $viewer = islandora_get_viewer($params, 'islandora_manuscript_viewers');
  if ($viewer) {
    $variables['viewer'] = $viewer;
  }
}

/**
 * Implements hook_preprocess_theme().
 */
function islandora_manuscript_preprocess_islandora_manuscript_page(array &$variables) {
  module_load_include('inc', 'islandora', 'includes/solution_packs');
  $object = $variables['object'];
  $results = $object->relationships->get(FEDORA_RELS_EXT_URI, 'isMemberOf');
  $result = reset($results);
  $variables['manuscript_object_id'] = $result ? $result['object']['value'] : FALSE;

  $params = array();
  if (isset($object['JP2']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['JP2'])) {
    // Get token to allow access to XACML protected datastreams.
    // Always use token authentication in case there is a global policy.
    module_load_include('inc', 'islandora', 'includes/authtokens');
    $token = islandora_get_object_token($object->id, 'JP2', 2);
    $jp2_url = url("islandora/object/{$object->id}/datastream/JP2/view", array(
      'absolute' => TRUE,
      'query' => array('token' => $token),
    ));
    $params['jp2_url'] = $jp2_url;
  }

  $viewer = islandora_get_viewer($params, 'islandora_manuscript_page_viewers');
  if ($viewer) {
    $variables['viewer'] = $viewer;
  }
}

/**
 * Prepares variables for islandora_manuscript_ead_display templates.
 *
 * Default template: islandora-manuscript-ead-display.tpl.php
 *
 * @param array $variables
 *   An associative array containing:
 *   - object: An AbstractObject containing an "EAD" datastream.
 *   - xslt_functions: An array of functions to allow the XSLT to run, as
 *     accepted by XSLTProcessor::registerPhpFunctions().
 *   - xslt_parameters: An associative array mapping namespace URIs to
 *     associative arrays of parameters proper.
 *   This function populates:
 *   - doc: A DOMDocument containing the parsed EAD datastream.
 *   - xslt_doc: A DOMDocument containing the parsed XSLT to run.
 */
function template_preprocess_islandora_manuscript_ead_display(&$variables) {
  $variables['xslt_parameters'][''] = (isset($variables['xslt_parameters']['']) ? $variables['xslt_parameters'][''] : array()) + array(
    "toc_string" => t('Table of Contents'),
    "container_string" => t('Containers'),
    "date_string" => t('Date'),
    "top_string" => t('Top'),
  );
  $variables['doc'] = $doc = new DOMDocument();
  $doc->loadXML($variables['object']['EAD']->content);
  $variables['xslt_doc'] = $xslt_doc = new DOMDocument();
  $xslt_doc->load(drupal_get_path('module', 'islandora_manuscript') . '/transforms/ead_to_html.xslt');
}

/**
 * Process variables for islandora_manuscript_ead_display templates.
 *
 * Default template: islandora-manuscript-ead-display.tpl.php
 *
 * @param array $variables
 *   An associative array containing:
 *   - object: An AbstractObject containing an "EAD" datastream.
 *   - xslt_functions: An array of functions to allow the XSLT to run, as
 *     accepted by XSLTProcessor::registerPhpFunctions().
 *   - xslt_parameters: An associative array mapping namespace URIs to
 *     associative arrays of parameters proper.
 *   - doc: A DOMDocument containing the parsed EAD datastream.
 *   - xslt_doc: A DOMDocument containing the parsed XSLT to run.
 *   This function populates:
 *   - processor: The XSLTProcessor instance which was used.
 *   - markup_doc: A DOMDocument containing the markup to output, after
 *     this function has run.
 */
function template_process_islandora_manuscript_ead_display(&$variables) {
  $variables['processor'] = $proc = new XSLTProcessor();
  $proc->importStylesheet($variables['xslt_doc']);
  foreach ($variables['xslt_parameters'] as $namespace_uri => $parameters) {
    $proc->setParameter($namespace_uri, $parameters);
  }
  $proc->registerPhpFunctions($variables['xslt_functions']);
  $variables['markup_doc'] = $proc->transformToDoc($variables['doc']);
}

/**
 * Callback used in XSLT to build a query URL.
 *
 * @param DOMElement[] $container_array
 *   An array containing a single DOMElement (this is how XSLTProcessor
 *   provides it) representing a "container" inside of an EAD document.
 * @param DOMElement[] $all
 *   An array containing all container elements in the given EAD document.
 *
 * @return string
 *   A string containing a URL to Solr search results for the given container.
 */
function islandora_manuscript_build_parented_query_url(array $container_array, array $all) {
  $object = menu_get_object('islandora_object', 2);
  if ($object) {
    $path = "islandora/object/{$object->id}/manuscript/manuscripts";
    list($container) = $container_array;
    return url($path, array(
      'query' => array(
        'f' => islandora_manuscript_build_subfile_query($container, $all),
      ),
    ));
  }
}

/**
 * Build a query to filter to the given container.
 *
 * @param DOMElement $container
 *   A DOMElement representing the container.
 *
 * @return string[]
 *   An array of strings representing Lucene queries... Likely to be used as
 *   filter queries.
 */
function islandora_manuscript_build_partial_query(DOMElement $container) {
  $subqueries = array();
  $field = islandora_manuscript_findingaid_get_solr_field($container->getAttribute('type'));

  $value = explode('-', trim($container->nodeValue));
  $count = count($value);
  if ($count >= 2) {
    $subqueries[] = format_string('!field:["!start" TO "!end"]', array(
      '!field' => $field,
      '!start' => $value[0],
      '!end' => $value[1],
    ));
  }
  elseif ($count === 1) {
    $subqueries[] = format_string('!field:"!value"', array(
      '!field' => $field,
      '!value' => $value[0],
    ));
  }

  return $subqueries;
}

/**
 * Build a query to select all items in a given part of a file.
 *
 * @param DOMElement $container
 *   An EAD container element for which to build a (part of a) query.
 * @param DOMElement[] $all
 *   An array of all container elements in the EAD doc...  'Cause the "parent"
 *   attribute can reference any container element.
 *
 * @return string[]
 *   An array of Lucene-syntax Solr queries.
 */
function islandora_manuscript_build_subfile_query(DOMElement $container, array $all = array()) {
  $subqueries = islandora_manuscript_build_partial_query($container);

  if ($container->hasAttribute('parent')) {
    foreach ($all as $element) {
      if ($element->getAttribute('id') == $container->getAttribute('parent')) {
        $subqueries = array_merge(islandora_manuscript_build_subfile_query($element, $all), $subqueries);
      }
    }
  }

  return $subqueries;
}

/**
 * Get the field for the given "type" of container.
 *
 * @param string $raw_type
 *   The raw type attribute value from the XML. A number of different formats
 *   have been seen in the wild, with boxes, for example:
 *   - Box
 *   - Boxes
 *   - box
 *   - boxes
 *   As a naive implementation, we lowercase and then compare at the beginning
 *   of the string for one of our recognized types, currently, just:
 *   - box
 *   - folder
 *
 * @return string|bool
 *   A string containing the name of a Solr field with which a query might be
 *   built, or FALSE if we do not have a mapping.
 */
function islandora_manuscript_findingaid_get_solr_field($raw_type) {
  $type = strtolower($raw_type);
  if (strpos($type, 'box') === 0) {
    return variable_get('islandora_manuscript_box_identifier_solr_field', 'mods_relatedItem_host_identifier_box_ms');
  }
  elseif (strpos($type, 'folder') === 0) {
    return variable_get('islandora_manuscript_folder_identifier_solr_field', 'mods_relatedItem_host_identifier_folder_ms');
  }
  else {
    watchdog('islandora_manuscript', 'Unrecognized type @type.', array('@type' => $raw_type));
    return FALSE;
  }
}

/**
 * Callback used in XSLT to build a query URL.
 *
 * @param DOMElement[] $containers
 *   An array containing a single DOMElement (this is how XSLTProcessor
 *   provides it) representing a "container" inside of an EAD document.
 *
 * @return string
 *   A string containing a URL to Solr search results for the given container.
 */
function islandora_manuscript_build_flat_query_url(array $containers) {
  $object = menu_get_object('islandora_object', 2);
  $parts = islandora_manuscript_build_flat_subfile_query($containers);
  if ($object && !empty($parts)) {
    $path = "islandora/object/{$object->id}/manuscript/manuscripts";
    return url($path, array(
      'query' => array(
        'f' => $parts,
      ),
    ));
  }
}

/**
 * Helper function to wrap the map and merge.
 *
 * @param DOMElement[] $containers
 *   An array of containers at the same component level in the EAD.
 *
 * @return string[]
 *   An array of strings representing Lucene queries.
 */
function islandora_manuscript_build_flat_subfile_query(array $containers) {
  // Array of array of Lucence queries...
  $parts = array_map('islandora_manuscript_build_partial_query', $containers);

  // Merge down to single array.
  return empty($parts) ? array() : call_user_func_array('array_merge', $parts);
}