<?php

/**
 * @file
 * Theme file for Newspaper solution pack. Includes preprocess functions.
 */

require_once dirname(__FILE__) . '/../includes/utilities.inc';

/**
 * Implements hook_preprocess().
 */
function theme_islandora_newspaper(array $variables) {
  drupal_add_js('misc/collapse.js');
  $object = $variables['object'];
  $issues = islandora_newspaper_get_issues($object);
  $grouped_issues = islandora_newspaper_group_issues($issues);
  $output = array(
    'controls' => array(
      '#theme' => 'links',
      '#attributes' => array(
        'class' => array('links', 'inline'),
      ),
      '#links' => array(
        array(
          'title' => t('Expand all months'),
          'href' => "javascript://void(0)",
          'html' => TRUE,
          'external' => TRUE,
          'attributes' => array(
            'onclick' => "Drupal.toggleFieldset(jQuery('fieldset.month.collapsed'));",
          ),
        ),
        array(
          'title' => t('Collapse all months'),
          'href' => "javascript://void(0)",
          'html' => TRUE,
          'external' => TRUE,
          'attributes' => array(
            'onclick' => "Drupal.toggleFieldset(jQuery('fieldset.month:not(.collapsed)'));",
          ),
        ),
      ),
    ),
    'tabs' => array(
      '#type' => 'vertical_tabs',
    ),
  );
  $tabs = &$output['tabs'];
  foreach ($grouped_issues as $year => $months) {
    $tabs[$year] = array(
      '#title' => $year,
      '#type' => 'fieldset',
    );
    foreach ($months as $month => $days) {
      $month_name = t("@date", array(
        "@date" => date("F", mktime(0, 0, 0, $month, 1, 2000)),
      ));
      $tabs[$year][$month] = array(
        '#title' => $month_name,
        '#type' => 'fieldset',
        '#attributes' => array(
          'class' => array('collapsible', 'collapsed', 'month'),
        ),
      );
      foreach ($days as $day => $issues) {
        foreach ($issues as $issue) {
          $tabs[$year][$month][$day][] = array(
            '#theme' => 'link',
            '#prefix' => '<div>',
            '#suffix' => '</div>',
            '#text' => t("@month @day, @year", array(
                '@year' => $year,
                '@month' => $month_name,
                '@day' => $day,
                )),
            '#path' => "islandora/object/{$issue['pid']}",
            '#options' => array(
              'attributes' => array(),
              'html' => FALSE,
            ),
          );
        }
      }
      ksort($tabs[$year][$month]);
    }
    ksort($tabs[$year]);
  }
  ksort($tabs);
  return drupal_render($output);
}

/**
 * Implements hook_preprocess().
 */
function islandora_newspaper_preprocess_islandora_newspaper_issue(array &$variables) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora', 'includes/solution_packs');
  $object = $variables['object'];
  $pages = islandora_paged_content_get_pages($object);
  $variables['pages'] = array_keys($pages);

  // Get viewer id.
  $viewer_id = islandora_get_viewer_id('islandora_newspaper_issue_viewers');
  if ($viewer_id) {
    $variables['viewer_id'] = $viewer_id;
  }
  else {
    $variables['viewer_id'] = '';
  }

  // Get viewer content.
  $params = array(
    'object' => $object,
    'pages' => $pages,
    'page_progression' => islandora_paged_content_get_page_progression($object),
  );
  $viewer = islandora_get_viewer($params, 'islandora_newspaper_issue_viewers');
  if ($viewer) {
    $variables['viewer'] = $viewer;
  }
}

/**
 * Theme the issue navigator.
 */
function theme_islandora_newspaper_issue_navigator(array $variables) {
  $object = $variables['object'];
  $newspaper = islandora_newspaper_get_newspaper($object);
  $newspaper = islandora_object_load($newspaper);
  $issues = islandora_newspaper_get_issues($newspaper);
  $issues = array_keys($issues);
  $index = array_search($object->id, $issues);
  $previous_issue = isset($issues[$index - 1]) ? $issues[$index - 1] : NULL;
  $next_issue = isset($issues[$index + 1]) ? $issues[$index + 1] : NULL;
  $links = array();
  if (isset($issues[$index - 1])) {
    $previous_issue = $issues[$index - 1];
    $links[] = array(
      'title' => t('Prev'),
      'href' => url("islandora/object/{$previous_issue}", array('absolute' => TRUE)),
    );
  }
  if (count($issues) > 1) {
    $links[] = array(
      'title' => '<strong>' . t('Issue') . '</strong>',
      'html' => TRUE,
    );
  }
  if (isset($issues[$index + 1])) {
    $next_issue = $issues[$index + 1];
    $links[] = array(
      'title' => t('Next'),
      'href' => url("islandora/object/{$next_issue}", array('absolute' => TRUE)),
    );
  }
  $links[] = array(
    'title' => t('All Issues'),
    'href' => url("islandora/object/{$newspaper->id}", array('absolute' => TRUE)),
  );
  $attributes = array('class' => array('links', 'inline'));
  return theme('links', array('links' => $links, 'attributes' => $attributes));
}

/**
 * Theme the page selector.
 */
function theme_islandora_newspaper_page_select(array $variables) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  $path = drupal_get_path('module', 'islandora_newspaper');
  drupal_add_js($path . '/js/islandora_newspaper.js');
  $object = $variables['object'];
  $results = $object->relationships->get(ISLANDORA_RELS_EXT_URI, 'isPageOf');
  $parent = islandora_object_load($results[0]['object']['value']);
  $pages = islandora_paged_content_get_pages($parent);
  $variables = array(
    '#options' => array(),
    '#attributes' => array('class' => array('page-select')),
  );
  foreach ($pages as $pid => $page) {
    $variables['#options'][$pid] = $page['page'];
  }
  $variables['#value'] = $object->id;
  $prefix = '<strong>' . t('Image') . ':</strong> ';
  return $prefix . t('!page_selector of @total', array(
      '!page_selector' => theme('select', array('element' => $variables)),
      '@total' => count($pages),
    ));
}

/**
 * Theme a newspaper pages controls.
 */
function islandora_newspaper_preprocess_islandora_newspaper_page_controls(array &$variables) {
  module_load_include('inc', 'islandora', 'includes/datastream');
  module_load_include('inc', 'islandora', 'includes/utilities');
  global $base_url;
  $view_prefix = '<strong>' . t('View:') . ' </strong>';
  $download_prefix = '<strong>' . t('Download:') . ' </strong>';
  $object = $variables['object'];
  $issue = islandora_newspaper_get_issue($object);
  $issue = islandora_object_load($issue);
  $controls = array(
    'page_select' => theme('islandora_newspaper_page_select', array('object' => $object)),
    'page_pager' => theme('islandora_paged_content_page_navigator', array('object' => $object)),
    'issue_pager' => theme('islandora_newspaper_issue_navigator', array('object' => $issue)),
  );
  // Text view.
  if (isset($object['OCR']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['OCR'])) {
    $url = islandora_datastream_get_url($object['OCR'], 'view');
    $attributes = array('attributes' => array('title' => t('View text')));
    $controls['text_view'] = $view_prefix . l(t('Text'), $url, $attributes);
  }
  // PDF download.
  if (isset($object['PDF']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['PDF'])) {
    $size = islandora_datastream_get_human_readable_size($object['PDF']);
    $text = t('PDF (@size)', array('@size' => $size));
    $url = islandora_datastream_get_url($object['PDF'], 'download');
    $attributes = array('attributes' => array('title' => t('Download PDF')));
    $controls['pdf_download'] = $download_prefix . l($text, $url, $attributes);
  }
  if (isset($object['JP2']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['JP2'])) {
    // JP2 download.
    $size = islandora_datastream_get_human_readable_size($object['JP2']);
    $text = t('JP2 (@size)', array('@size' => $size));
    $url = islandora_datastream_get_url($object['JP2'], 'download');
    $attributes = array('attributes' => array('title' => t('Download JP2')));
    $controls['jp2_download'] = $download_prefix . l($text, $url, $attributes);

    // JP2 Clip.
    $jp2_ds = $object['JP2'];
    $module_path = drupal_get_path('module', 'islandora_newspaper');
    $path = "$module_path/images/clip_icon.png";
    $image = theme('image', array('path' => $path, 'attributes' => array()));
    $url = "islandora/object/{$object->id}/print";
    $attributes = array(
      'attributes' => array(
        'title' => t('Clip Image'),
        'id' => 'clip',
      ),
      'html' => TRUE,
    );
    $controls['clip'] = l($image, $url, $attributes);
  }
  // TIFF download.
  if (isset($object['OBJ']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['OBJ'])) {
    $size = islandora_datastream_get_human_readable_size($object['OBJ']);
    $text = t('TIFF (@size)', array('@size' => $size));
    $url = islandora_datastream_get_url($object['OBJ'], 'download');
    $attributes = array('attributes' => array('title' => t('Download TIFF')));
    $controls['tiff_download'] = $download_prefix . l($text, $url, $attributes);
  }
  $variables['controls'] = $controls;
}

/**
 * Implements hook_preprocess().
 */
function islandora_newspaper_preprocess_islandora_newspaper_page(array &$variables) {
  module_load_include('inc', 'islandora', 'includes/datastream');
  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('inc', 'islandora', 'includes/solution_packs');
  $object = $variables['object'];

  $params = array();
  if (isset($object['JP2']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['JP2'])) {
    // Get token to allow access to XACML protected datastreams if necessary.
    // Always use token authentication in case there is a global policy.
    module_load_include('inc', 'islandora', 'includes/authtokens');
    $token = islandora_get_object_token($object->id, 'JP2', 2);
    $jp2_url = url("islandora/object/{$object->id}/datastream/JP2/view",
      array(
        'absolute' => TRUE,
        'query' => array('token' => $token),
      ));

    // Display newspaper.
    $params['jp2_url'] = $jp2_url;
  }
  $viewer = islandora_get_viewer($params, 'islandora_newspaper_page_viewers', $object);
  $variables['content'] = '';
  if ($viewer) {
    $variables['content'] = $viewer;
  }
  // If no viewer is configured just show the jpeg.
  elseif (isset($object['JPG']) && islandora_datastream_access(ISLANDORA_VIEW_OBJECTS, $object['JPG'])) {
    $params = array(
      'title' => $object->label,
      'path' => url("islandora/object/{$object->id}/datastream/JPG/view"),
    );
    $variables['content'] = theme('image', $params);
  }
  else {
    $variables['content'] = NULL;
  }
}