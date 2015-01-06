<?php

define('PLATFORM' , 'Drupal Commerce');
define('DEVELOPER' , 'Name Developer');
define('STORE_NAME' , 'Name of store');
define('STORE_OWNER' , 'Name of store owner');
define('STORE_OWNER_EMAIL_ADDRESS' , 'Email address of store owner');
define('STORE_STATE' , 'State');
define('STORE_COUNTRY' , 'Country');
define('STORE_WEBSITE' , 'http://www.example.com');
define('STORE_TIME_ZONE', 'UTC');
define('STORE_CURRENCY', 'USD');
define('REQUIRE_SECURE', true);

function commerce_shipworks_endpoint() {

  $moduleVersion = "3.0.0";
  $schemaVersion = "1.0.0";

  header("Last-Modified: " . gmdate("D, d M Y H:i:s") . " GMT");

  // HTTP/1.1
  header("Content-Type: text/xml;charset=utf-8");
  header("Cache-Control: no-store, no-cache, must-revalidate");
  header("Cache-Control: post-check=0, pre-check=0", false);

  // HTTP/1.0
  header("Pragma: no-cache");

  $secure = (isset($_SERVER['HTTPS']) && ($_SERVER['HTTPS'] == 'on' || $_SERVER['HTTPS'] == '1'));

  // Open the XML output and root
  _writeXmlDeclaration();
  _writeStartTag("ShipWorks", array("moduleVersion" => $moduleVersion, "schemaVersion" => $schemaVersion));

  // Enforse SSL
  if (!$secure && REQUIRE_SECURE) {
    _outputError(10, 'A secure (https://) connection is required.');
  }

  else {
    $action = (isset($_REQUEST['action']) ? $_REQUEST['action'] : '');
    switch (strtolower($action)) {
      case 'getmodule': _Action_GetModule(); break;
      case 'getstore': _Action_GetStore(); break;
      case 'getcount': _Action_GetCount(); break;
      case 'getorders': _Action_GetOrders(); break;
      case 'getstatuscodes': _Action_GetStatusCodes(); break;
      case 'updatestatus': _Action_UpdateStatus(); break;
      case 'updateshipment': _Action_UpdateShipment(); break;
      default: _outputError(70, "'$action' not supported");
    }
  }

    // Close the output
  _writeCloseTag("ShipWorks");

  // Exit so we don't do any extra/unnecessary rendering.
  drupal_exit();

}

// Get module data
function _Action_GetModule() {
  _writeStartTag("Module");

    _writeElement("Platform", PLATFORM);
    _writeElement("Developer", DEVELOPER);

    _writeStartTag("Capabilities");
      _writeElement("DownloadStrategy", "ByModifiedTime");
      _writeFullElement("OnlineCustomerID", "", array("supported" => "false"));
      _writeFullElement("OnlineStatus", "", array("supported" => "true", "dataType" => "text", "supportsComments" => "false" ));
      _writeFullElement("OnlineShipmentUpdate", "", array("supported" => "true"));
    _writeCloseTag("Capabilities");

   _writeCloseTag("Module");
}

// Write store data
function _Action_GetStore() {
  _writeStartTag("Store");
    _writeElement("Name", STORE_NAME);
    _writeElement("CompanyOrOwner", STORE_OWNER);
    _writeElement("Email", STORE_OWNER_EMAIL_ADDRESS);
    _writeElement("State", STORE_STATE);
    _writeElement("Country", STORE_COUNTRY);
    _writeElement("Website", STORE_WEBSITE);
  _writeCloseTag("Store");
}

function _Action_GetStatusCodes() {
  _writeStartTag("StatusCodes");

  $codes['canceled'] = array(
    'name' => 'canceled',
    'title' => t('Canceled'),
  );
  $codes['pending'] = array(
    'name' => 'pending',
    'title' => t('Pending'),
  );
  $codes['processing'] = array(
    'name' => 'processing',
    'title' => t('Processing'),
  );
  $codes['completed'] = array(
    'name' => 'completed',
    'title' => t('Completed'),
  );

  while (list($key, $val) = each($codes))
  {
    _writeStartTag("StatusCode");
      _writeElement("Code", $val['name']);
      _writeElement("Name", $val['title']);
    _writeCloseTag("StatusCode");
  }

  _writeCloseTag("StatusCodes");
}

// Get the count of orders greater than the start ID
function _Action_GetCount() {
  // Set default timezone
  date_default_timezone_set(STORE_TIME_ZONE);

  $start = '1970-01-01T00:00:00';

  if (isset($_REQUEST['start'])) {
    $start = $_REQUEST['start'];
  }

  // Write the params for easier diagnostics
  _writeStartTag("Parameters");
      _writeElement("Start", $start);
  _writeCloseTag("Parameters");

  $ordersQuery = db_select('commerce_order', 'o');
  $ordersQuery->join('field_data_field_payment_status', 'ps', 'o.order_id = ps.entity_id');
  $ordersQuery->fields('o', array('order_id'));
  $ordersQuery->where('o.changed > :changed', array(':changed' => strtotime($start)));
  $ordersQuery->condition('ps.field_payment_status_value', array('paid'), 'IN');
  $ordersQuery->condition(
                  db_or()
                    ->condition('o.status', array('processing'), 'IN')
                    ->condition('o.status', array('pending'), 'IN')
  );
  $count = $ordersQuery->countQuery()->execute()->fetchField();

  _writeElement("OrderCount", (int) $count);
}

// Get all orders greater than the given start id, limited by max count
function _Action_GetOrders() {
  // Set default timezone
  date_default_timezone_set(STORE_TIME_ZONE);

  $start = '1970-01-01T00:00:00';
  $maxcount = 50;

  if (isset($_REQUEST['start'])) {
    $start = $_REQUEST['start'];
  }

  // Write the params for easier diagnostics
  _writeStartTag("Parameters");
    _writeElement("StartGMT", $start);

  if (isset($_REQUEST['maxcount'])) {
    $maxcount = $_REQUEST['maxcount'];
  }

  // Only get orders through 2 seconds ago.
  $end = date("Y-m-d\TH:i:s", time() - 2);

    _writeElement("StartLocal", $start);
    _writeElement("End", $end);
    _writeElement("MaxCount", $maxcount);
  _writeCloseTag("Parameters");

  _writeStartTag("Orders");

  $ordersQuery = db_select('commerce_order', 'o');
  $ordersQuery->join('field_data_field_payment_status', 'ps', 'o.order_id = ps.entity_id');
  $ordersQuery->fields('o', array('order_id', 'changed'));
  $ordersQuery->where('o.changed > :changed', array(':changed' => strtotime($start)));
  $ordersQuery->condition('ps.field_payment_status_value', array('paid'), 'IN');
  $ordersQuery->condition(
                  db_or()
                    ->condition('o.status', array('processing'), 'IN')
                    ->condition('o.status', array('pending'), 'IN')
  );
  $result = $ordersQuery->execute();

  foreach ($result as $row) {
    // Load order
    $order = commerce_order_load($row->order_id);
    $order_wrapper = entity_metadata_wrapper('commerce_order', $order);

    _WriteOrder($order_wrapper);
  }
    _writeCloseTag("Orders");
}

function _WriteOrder($order_wrapper) {

  // Shipping Method
  foreach ($order_wrapper->commerce_line_items as $index => $line_item_wrapper) {
    if ($line_item_wrapper->type->value() == 'shipping') {
      $shipping_method = $line_item_wrapper->line_item_label->value();
    }
  }

  // Notes
  $admin_notes = $order_wrapper->field_order_admin_notes->value();
  $notes = strip_tags($admin_notes['value']);

  _writeStartTag("Order");

    _writeElement("OrderNumber", $order_wrapper->order_id->value());
    _writeElement("OrderDate", date("Y-m-d\TH:i:s", $order_wrapper->created->value()));
    _writeElement("LastModified", date("Y-m-d\TH:i:s", $order_wrapper->changed->value()));
    _writeElement("ShippingMethod", $shipping_method);
    _writeElement("StatusCode", $order_wrapper->status->value());

    _writeStartTag("Notes");
      _WriteNote($notes, date("Y-m-d\TH:i:s", $order_wrapper->changed->value()));
    _writeCloseTag("Notes");

    _writeStartTag("ShippingAddress");
      _writeElement("FullName", $order_wrapper->commerce_customer_shipping->commerce_customer_address->name_line->value());
      _writeElement("Company", $order_wrapper->commerce_customer_shipping->commerce_customer_address->organisation_name->value());
      _writeElement("Street1", $order_wrapper->commerce_customer_shipping->commerce_customer_address->thoroughfare->value());
      _writeElement("Street2", $order_wrapper->commerce_customer_shipping->commerce_customer_address->premise->value());
      _writeElement("Street3", "");
      _writeElement("City", $order_wrapper->commerce_customer_shipping->commerce_customer_address->locality->value());
      _writeElement("State", $order_wrapper->commerce_customer_shipping->commerce_customer_address->administrative_area->value());
      _writeElement("PostalCode", $order_wrapper->commerce_customer_shipping->commerce_customer_address->postal_code->value());
      _writeElement("Country", $order_wrapper->commerce_customer_shipping->commerce_customer_address->country->value());
      _writeElement("Phone", "");
      _writeElement("Email", "");
    _writeCloseTag("ShippingAddress");

    _writeStartTag("BillingAddress");
      _writeElement("FullName", $order_wrapper->commerce_customer_billing->commerce_customer_address->name_line->value());
      _writeElement("Company", $order_wrapper->commerce_customer_billing->commerce_customer_address->organisation_name->value());
      _writeElement("Street1", $order_wrapper->commerce_customer_billing->commerce_customer_address->thoroughfare->value());
      _writeElement("Street2", $order_wrapper->commerce_customer_billing->commerce_customer_address->premise->value());
      _writeElement("Street3", "");
      _writeElement("City", $order_wrapper->commerce_customer_billing->commerce_customer_address->locality->value());
      _writeElement("State", $order_wrapper->commerce_customer_billing->commerce_customer_address->administrative_area->value());
      _writeElement("PostalCode", $order_wrapper->commerce_customer_billing->commerce_customer_address->postal_code->value());
      _writeElement("Country", $order_wrapper->commerce_customer_billing->commerce_customer_address->country->value());
      _writeElement("Phone", $order_wrapper->field_order_phone_number->value());
      _writeElement("Email", $order_wrapper->mail->value());
    _writeCloseTag("BillingAddress");

    _WriteOrderItems($order_wrapper);

    _WriteOrderTotals($order_wrapper);

    _writeStartTag("Debug");
      _writeElement("LastModifiedLocal", date("Y-m-d\TH:i:s", $order_wrapper->changed->value()));
    _writeCloseTag("Debug");
  _writeCloseTag("Order");
}

// Write XML for all products for the given order
function _WriteOrderItems($order_wrapper) {

  _writeStartTag("Items");

  // Loop through product line items
  foreach ($order_wrapper->commerce_line_items as $index => $line_item_wrapper) {
    if ($line_item_wrapper->type->value() == 'product') {

      // Unit price
      $line_item = $line_item_wrapper->commerce_unit_price->data->value();
      $unit_price = 0;

      // Cost
      $cost = $line_item_wrapper->commerce_product->commerce_price->amount->value();

      foreach ($line_item['components'] as $delta => $data) {
        if ($data['name'] == 'base_price' || $data['name'] == 'discount') {
          // Assemble unit price minor units
          $unit_price += $data['price']['amount'];
        }
      }

    // Weight
    $weight_amount = round($line_item_wrapper->commerce_product->field_weight->weight->value(), 2);

    _writeStartTag("Item");
      _writeElement("ProductID", $line_item_wrapper->commerce_product->product_id->value());
      _writeElement("SKU", $line_item_wrapper->commerce_product->sku->value());
      _writeElement("Code", $line_item_wrapper->commerce_product->sku->value());
      _writeElement("Name", $line_item_wrapper->commerce_product->title->value());
      _writeElement("Quantity", round($line_item_wrapper->quantity->value()));
      _writeElement("UnitPrice", commerce_currency_amount_to_decimal($unit_price, STORE_CURRENCY));
      _writeElement("UnitCost", commerce_currency_amount_to_decimal($cost, STORE_CURRENCY));
      _writeElement("Weight", $weight_amount);
    _writeCloseTag("Item");
  }
}
  _writeCloseTag("Items");
}

// Write all totals lines for the order
function _WriteOrderTotals($order_wrapper) {

  $order_total = $order_wrapper->commerce_order_total->data->value();

  _writeStartTag("Totals");

  foreach ($order_total['components'] as $delta => $data) {
    if (preg_match('/^commerce_coupon/', $data['name'])) {
      $value = commerce_currency_amount_to_decimal(abs($data['price']['amount']), STORE_CURRENCY);
      $attributes = array(
        "name" => 'Coupon',
        "class" => 'COUPON',
        "impact" => 'subtract',
      );
      _writeFullElement("Total", $value, $attributes);
    }
    if ($data['name'] == 'tax|illinois_tax') {
      $value = commerce_currency_amount_to_decimal($data['price']['amount'], STORE_CURRENCY);
      $attributes = array(
        "name" => 'Sales Tax',
        "class" => 'TAX',
        "impact" => 'add',
      );
      _writeFullElement("Total", $value, $attributes);
    }
    if ($data['name'] == 'shipping') {
      $value = commerce_currency_amount_to_decimal($data['price']['amount'], STORE_CURRENCY);
      $attributes = array(
        "name" => 'Shipping',
        "class" => 'SHIPPING',
        "impact" => 'add',
      );
      _writeFullElement("Total", $value, $attributes);
    }
  }
  _writeCloseTag("Totals");
}

function _WriteNote($noteText, $dateAdded) {
  if (strlen($noteText) > 0) {
    $attributes = array("public" => "false",
      "date" => $dateAdded);

    _writeFullElement("Note", $noteText, $attributes);
  }
}

function _Action_UpdateStatus() {
  if (!isset($_REQUEST['order']) || !isset($_REQUEST['status'])) {
    _outputError(40, "Not all parameters supplied.");
    return;
  }

  if (is_int($_REQUEST['order'])) {
    $order_id = $_REQUEST['order'];
  }
  else {
    _outputError(40, "Not a valid order id.");
  }
  
  switch($_REQUEST['status']) {

    case 'canceled':
      $status = $_REQUEST['status'];
      break;
      
    case 'pending':
      $status = $_REQUEST['status'];
      break;

    case 'processing':
      $status = $_REQUEST['status'];
      break;

    case 'completed':
      $status = $_REQUEST['status'];
      break;

    default:
      _outputError(40, "Not a valid order status.");
    }

  $order = commerce_order_load($order_id);
  

  // Update order status
  commerce_order_status_update($order, $status, $skip_save = FALSE, $revision = TRUE, $log = '');

  echo "<UpdateSuccess/>";
}

function _Action_UpdateShipment() {
  if (!isset($_REQUEST['order']) || !isset($_REQUEST['tracking'])) {
    _outputError(40, "Not all parameters supplied.");
    return;
  }

  if (is_int($_REQUEST['order'])) {
    $order_id = $_REQUEST['order'];
  
    $order = commerce_order_load($order_id);
 
    $order_wrapper = entity_metadata_wrapper('commerce_order', $order);

    $tracking = $_REQUEST['tracking'];

    $tracking_number = str_replace(' ', '', strtoupper($tracking));

    $tracking_numbers = array();

    foreach ($order_wrapper->field_tracking_numbers as $delta => $item) {
      $tracking_numbers[] = $item->value();
    }

    if (!in_array($tracking_number, $tracking_numbers)) {
      $order_wrapper->field_tracking_numbers[$delta+1]->set($tracking_number);
      $order_wrapper->save();
      echo "<UpdateSuccess/>";
    }
  }
  else {
    _outputError(40, "Not a valid order id.");
  }
}

// Output xml declaration
function _writeXmlDeclaration() {
    echo "<?xml version=\"1.0\" standalone=\"yes\" ?>";
}

// Output start tag
function _writeStartTag($tag, $attributes = null) {
  echo '<' . $tag;

  if ($attributes != null) {
    echo ' ';

    foreach ($attributes as $name => $attribValue) {
      echo _toUtf8($name. '="'. htmlspecialchars($attribValue). '" ');
    }
  }

  echo '>';
}

// Output the given tag\value pair
function _writeElement($tag, $value) {
  _writeStartTag($tag);
  echo _toUtf8(htmlspecialchars($value));
  _writeCloseTag($tag);
}

// Outputs the given name/value pair as an xml tag with attributes
function _writeFullElement($tag, $value, $attributes) {
  echo _toUtf8('<'. $tag. ' ');

  foreach ($attributes as $name => $attribValue) {
    echo _toUtf8($name. '="'. htmlspecialchars($attribValue). '" ');
  }
  echo '>';
  echo _toUtf8(htmlspecialchars($value));
  _writeCloseTag($tag);
}

// Output close tag
function _writeCloseTag($tag) {
  echo _toUtf8('</' . $tag . '>');
}

// Convert to UTF-8
function _toUtf8($string) {
  return iconv("ISO-8859-1", "UTF-8//TRANSLIT", $string);
}

// Function used to output an error and quit.
function _outputError($code, $error) {
  _writeStartTag("Error");
  _writeElement("Code", $code);
  _writeElement("Description", $error);
  _writeCloseTag("Error");
}
