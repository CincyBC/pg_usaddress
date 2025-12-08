# pg_usaddress

A PostgreSQL extension for parsing US addresses, based on the [datamade/usaddress](https://github.com/datamade/usaddress) Python package. This extension uses Conditional Random Fields (CRF) to parse address strings into their component parts (Street Number, Street Name, City, State, Zip, etc.) directly within PostgreSQL.

## Features

- **CRF-based Parsing**: Uses the same underlying statistical model as the Python `usaddress` package.
- **PostgreSQL Native**: Runs directly in the database C-extension for high performance.
- **Zero Python Runtime Dependency**: The model is loaded by a C wrapper around CRFsuite, requiring no Python environment at runtime.

## Installation

### With Docker

The easiest way to try out the extension is using Docker Compose.

1.  **Build and Start**:
    ```bash
    docker-compose up --build -d
    ```

2.  **Verify Installation**:
    ```bash
    docker-compose exec db psql -U postgres -c "CREATE EXTENSION IF NOT EXISTS pg_usaddress;"
    docker-compose exec db psql -U postgres -c "SELECT * FROM parse_address_crf('123 Main St, Chicago, IL 60601');"
    ```

### Manual Installation

To install directly on your system, you need PostgreSQL development headers and a C compiler.

1.  **Prerequisites**:
    *   PostgreSQL 14+ (mostly tested on 17)
    *   `gcc` or `clang`
    *   `make`

    On Debian/Ubuntu:
    ```bash
    sudo apt-get install postgresql-server-dev-17 build-essential
    ```

2.  **Build and Install**:
    ```bash
    make
    sudo make install
    ```

3.  **Enable in Database**:
    ```sql
    CREATE EXTENSION pg_usaddress;
    ```

## Usage

### `parse_address_crf(text)`

Returns a table of tokens and their predicted labels.

```sql
SELECT * FROM parse_address_crf('1600 Pennsylvania Ave NW, Washington, DC 20500');
```

**Output:**

| token | label |
|-------|-------|
| 1600 | AddressNumber |
| Pennsylvania | StreetName |
| Ave | StreetNamePostType |
| NW | StreetNamePostDirectional |
| Washington | PlaceName |
| DC | StateName |
| 20500 | ZipCode |

### `parse_address_crf_normalized(text)`

Returns a table of tokens and labels, but applies USPS standardization:
*   Uppercases all tokens.
*   Maps street types (e.g., "Street" -> "ST") and occupancy types (e.g., "Suite" -> "STE") to USPS abbreviations.
*   Removes all commas.

```sql
SELECT * FROM parse_address_crf_normalized('100 North Michigan Avenue, Suite 200, Chicago, IL 60611');
```

**Output:**

| token | label |
|-------|-------|
| 100 | AddressNumber |
| NORTH | StreetNamePreDirectional |
| MICHIGAN | StreetName |
| AVE | StreetName |
| STE | OccupancyType |
| 200 | OccupancyIdentifier |
| CHICAGO | PlaceName |
| IL | StateName |
| 60611 | ZipCode |

### `crf_full_address_normalized(text)`

Returns a single string with the fully normalized address, concatenating components with proper spacing and placing a comma between the city and state.

```sql
SELECT crf_full_address_normalized('100 North Michigan Avenue, Suite 200, Chicago, IL 60611');
```

**Output:**

```text
100 NORTH MICHIGAN AVE STE 200 CHICAGO, IL 60611
```

## Model Training

If you want to retrain the underlying CRF model with your own data:

1.  **Install Dependencies**:
    You need `uv` or `pip` to install the training script dependencies.
    ```bash
    # Using uv (recommended)
    uv sync
    ```

2.  **Prepare Data**:
    Place your training data in XML format in `training_data/`. The format should match the `usaddress` training data structure.

3.  **Train Model**:
    Run the Python training script. This script generates features compatible with the C extension's feature extractor.
    ```bash
    uv run tools/train_model.py
    ```
    
    This will generate a new `include/usaddr.crfsuite` file.

4.  **Rebuild Extension**:
    After generating a new model, you need to reinstall the extension so the new model file is copied to the PostgreSQL extension directory.
    ```bash
    sudo make install
    ```
