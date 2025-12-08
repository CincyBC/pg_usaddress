-- Regression tests for pg_usaddress

-- Load the extension
CREATE EXTENSION pg_usaddress;

-- =====================================================
-- Section 1: Basic Address Parsing (parse_address_crf)
-- =====================================================

-- Test 1: Simple street address
SELECT * FROM parse_address_crf('123 Main Street');

-- Test 2: Address with city
SELECT * FROM parse_address_crf('456 Oak Avenue, Chicago');

-- Test 3: Full address with state and zip
SELECT * FROM parse_address_crf('789 Elm Boulevard, New York, NY 10001');

-- Test 4: Address with directional prefix
SELECT * FROM parse_address_crf('100 North Michigan Avenue, Chicago, IL 60611');

-- Test 5: Address with directional suffix
SELECT * FROM parse_address_crf('200 Broadway East, Seattle, WA 98102');

-- Test 6: Address with apartment/unit
SELECT * FROM parse_address_crf('350 Park Avenue, Apt 5B, New York, NY 10022');

-- Test 7: Address with suite
SELECT * FROM parse_address_crf('1600 Pennsylvania Avenue NW, Suite 100, Washington, DC 20500');

-- Test 8: PO Box address
SELECT * FROM parse_address_crf('PO Box 1234, Los Angeles, CA 90001');

-- Test 9: Highway address
SELECT * FROM parse_address_crf('12345 Highway 101, San Francisco, CA 94102');

-- Test 10: Rural route
SELECT * FROM parse_address_crf('RR 2 Box 45, Springfield, IL 62701');

-- =====================================================
-- Section 2: JSON Tagging (tag_address_crf)
-- =====================================================

-- Test 11: Simple tagged address
SELECT tag_address_crf('123 Main Street');

-- Test 12: Full tagged address
SELECT tag_address_crf('456 Oak Avenue, Chicago, IL 60601');

-- Test 13: Complex tagged address  
SELECT tag_address_crf('100 North Michigan Avenue, Suite 200, Chicago, IL 60611');

-- =====================================================
-- Section 3: Column Parsing (parse_address_crf_cols)
-- =====================================================

-- Test 14: Simple column parsing
SELECT * FROM parse_address_crf_cols('123 Main Street');

-- Test 15: Full address column parsing
SELECT * FROM parse_address_crf_cols('456 Oak Avenue, Chicago, IL 60601');

-- Test 16: Complex address column parsing
SELECT * FROM parse_address_crf_cols('100 North Michigan Avenue, Suite 200, Chicago, IL 60611');

-- Test 17: Address with zip+4
SELECT * FROM parse_address_crf_cols('500 5th Avenue, New York, NY 10110-0001');

-- =====================================================
-- Section 4: Edge Cases
-- =====================================================

-- Test 18: Numbered street name
SELECT * FROM parse_address_crf('42 West 42nd Street, New York, NY 10036');

-- Test 19: Multiple word street name
SELECT * FROM parse_address_crf('1 Martin Luther King Jr Boulevard, Atlanta, GA 30303');

-- Test 20: All uppercase input
SELECT * FROM parse_address_crf('123 MAIN STREET, CHICAGO, IL 60601');

-- Test 21: Mixed case input
SELECT * FROM parse_address_crf('123 Main STREET, chicago, IL 60601');

-- Test 22: Extra whitespace
SELECT * FROM parse_address_crf('  123   Main   Street  ,  Chicago  ,  IL  60601  ');

-- =====================================================
-- Section 5: Real-world Examples
-- =====================================================

-- Test 23: Empire State Building
SELECT * FROM parse_address_crf('350 5th Avenue, New York, NY 10118');

-- Test 24: White House
SELECT * FROM parse_address_crf('1600 Pennsylvania Avenue NW, Washington, DC 20500');

-- Test 25: Willis Tower (formerly Sears Tower)
SELECT * FROM parse_address_crf('233 South Wacker Drive, Chicago, IL 60606');

-- =====================================================
-- Section 6: Normalized Address Parsing
-- =====================================================

-- Test 26: Normalized simple address (STREET -> ST, uppercase)
SELECT * FROM parse_address_crf_normalized('123 Main Street');

-- Test 27: Normalized with Avenue (AVENUE -> AVE)
SELECT * FROM parse_address_crf_normalized('456 Oak Avenue, Chicago, IL 60601');

-- Test 28: Normalized with Suite (SUITE -> STE) and comma between city/state
SELECT * FROM parse_address_crf_normalized('100 North Michigan Avenue, Suite 200, Chicago, IL 60611');

-- =====================================================
-- Section 7: Full Address Normalization
-- =====================================================

-- Test 29: Full normalized address
SELECT crf_full_address_normalized('123 Main Street, Chicago, IL 60601');

-- Test 30: Full normalized with suite
SELECT crf_full_address_normalized('100 North Michigan Avenue, Suite 200, Chicago, IL 60611');

-- Clean up
DROP EXTENSION pg_usaddress;
