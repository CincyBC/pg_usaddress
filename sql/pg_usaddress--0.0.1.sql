/* pg_usaddress--0.0.1.sql */

CREATE OR REPLACE FUNCTION parse_address_crf(input_text text)
RETURNS TABLE(token text, label text)
AS '$libdir/pg_usaddress', 'parse_address_crf'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION parse_address_crf(text) IS 'Parse an address into its components using a CRF model';

CREATE OR REPLACE FUNCTION parse_address_crf_normalized(input_text text)
RETURNS TABLE(token text, label text)
AS '$libdir/pg_usaddress', 'parse_address_crf_normalized'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION parse_address_crf_normalized(text) IS 'Parse and normalize an address with USPS standardization';

CREATE OR REPLACE FUNCTION tag_address_crf(input_text text)
RETURNS jsonb
AS '$libdir/pg_usaddress', 'tag_address_crf'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION tag_address_crf(text) IS 'Tag an address with its components using a CRF model';


CREATE TYPE parsed_address_crf AS (
    address_number character varying(50),
    address_number_prefix character varying(20),
    address_number_suffix character varying(20),
    street_name_pre_modifier character varying(50),
    street_name_pre_directional character varying(20),
    street_name_pre_type character varying(50),
    street_name character varying(255),
    street_name_post_type character varying(50),
    street_name_post_directional character varying(20),
    street_name_post_modifier character varying(50),
    subaddress_identifier character varying(100),
    subaddress_type character varying(50),
    place_name character varying(255),
    state_name character varying(100),
    zip_code character varying(20),
    zip_plus4 character varying(10),
    usps_box_type character varying(50),
    usps_box_id character varying(50),
    building_name character varying(255),
    occupancy_type character varying(50),
    occupancy_identifier character varying(100)
);

CREATE FUNCTION parse_address_crf_cols(input_text text)
RETURNS parsed_address_crf
AS '$libdir/pg_usaddress', 'parse_address_crf_cols'
LANGUAGE C IMMUTABLE STRICT;
COMMENT ON FUNCTION parse_address_crf_cols(text) IS 'Parse an address into standardized columns using a CRF model';

-- Function to get a fully normalized, concatenated address string
CREATE OR REPLACE FUNCTION crf_full_address_normalized(input_text text)
RETURNS text
LANGUAGE SQL IMMUTABLE STRICT
AS $$
  WITH parsed AS (
    SELECT token, label FROM parse_address_crf_normalized(input_text)
  ),
  aggregated AS (
    SELECT 
      string_agg(token, ' ') FILTER (WHERE label = 'AddressNumber') AS address_number,
      string_agg(token, ' ') FILTER (WHERE label = 'StreetNamePreDirectional') AS street_pre_dir,
      string_agg(token, ' ') FILTER (WHERE label = 'StreetName') AS street_name,
      string_agg(token, ' ') FILTER (WHERE label = 'StreetNamePostType') AS street_post_type,
      string_agg(token, ' ') FILTER (WHERE label = 'StreetNamePostDirectional') AS street_post_dir,
      string_agg(token, ' ') FILTER (WHERE label = 'OccupancyType') AS occupancy_type,
      string_agg(token, ' ') FILTER (WHERE label = 'OccupancyIdentifier') AS occupancy_id,
      string_agg(token, ' ') FILTER (WHERE label = 'PlaceName') AS place_name,
      string_agg(token, ' ') FILTER (WHERE label = 'StateName') AS state_name,
      string_agg(token, ' ') FILTER (WHERE label = 'ZipCode') AS zip_code
    FROM parsed
  )
  SELECT 
    concat_ws(' ',
      address_number,
      street_pre_dir,
      street_name,
      street_post_type,
      street_post_dir,
      occupancy_type,
      occupancy_id,
      CASE 
        WHEN place_name IS NOT NULL AND state_name IS NOT NULL THEN place_name || ','
        ELSE place_name
      END,
      state_name,
      zip_code
    )
  FROM aggregated;
$$;
COMMENT ON FUNCTION crf_full_address_normalized(text) IS 'Parse, normalize, and concatenate an address with comma between city and state';
