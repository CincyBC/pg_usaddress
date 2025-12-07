#!/usr/bin/env python3
"""
Train a CRFSuite model for address parsing.

This script reads XML training data and produces a .crfsuite model file
that can be used by the C extension for address parsing.

Usage:
    python3 tools/train_model.py

The output model is saved to include/usaddr.crfsuite
"""

import os
import re
import glob
import pycrfsuite
from xml.etree import ElementTree as ET

# Directions and street names for feature extraction
DIRECTIONS = {
    'n', 's', 'e', 'w', 'ne', 'nw', 'se', 'sw',
    'north', 'south', 'east', 'west',
    'northeast', 'northwest', 'southeast', 'southwest'
}

STREET_NAMES = {
    'allee', 'alley', 'ally', 'aly', 'anex', 'annex', 'annx', 'anx', 'arc',
    'arcade', 'av', 'ave', 'aven', 'avenu', 'avenue', 'avn', 'avnue', 'bayoo',
    'bayou', 'bch', 'beach', 'bend', 'bg', 'bgs', 'bl', 'blf', 'blfs', 'bluf',
    'bluff', 'bluffs', 'blvd', 'bnd', 'bot', 'bottm', 'bottom', 'boul',
    'boulevard', 'boulv', 'br', 'branch', 'brdge', 'brg', 'bridge', 'brk',
    'brks', 'brnch', 'brook', 'brooks', 'btm', 'burg', 'burgs', 'byp', 'bypa',
    'bypas', 'bypass', 'byps', 'byu', 'camp', 'canyn', 'canyon', 'cape',
    'causeway', 'causwa', 'causway', 'cen', 'cent', 'center', 'centers',
    'centr', 'centre', 'ci', 'cir', 'circ', 'circl', 'circle', 'circles',
    'cirs', 'ck', 'clb', 'clf', 'clfs', 'cliff', 'cliffs', 'club', 'cmn',
    'cmns', 'cmp', 'cnter', 'cntr', 'cnyn', 'common', 'commons', 'cor',
    'corner', 'corners', 'cors', 'course', 'court', 'courts', 'cove', 'coves',
    'cp', 'cpe', 'cr', 'crcl', 'crcle', 'crecent', 'creek', 'cres', 'crescent',
    'cresent', 'crest', 'crk', 'crossing', 'crossroad', 'crossroads', 'crscnt',
    'crse', 'crsent', 'crsnt', 'crssing', 'crssng', 'crst', 'crt', 'cswy',
    'ct', 'ctr', 'ctrs', 'cts', 'curv', 'curve', 'cv', 'cvs', 'cyn', 'dale',
    'dam', 'div', 'divide', 'dl', 'dm', 'dr', 'driv', 'drive', 'drives', 'drs',
    'drv', 'dv', 'dvd', 'est', 'estate', 'estates', 'ests', 'ex', 'exp',
    'expr', 'express', 'expressway', 'expw', 'expy', 'ext', 'extension',
    'extensions', 'extn', 'extnsn', 'exts', 'fall', 'falls', 'ferry', 'field',
    'fields', 'flat', 'flats', 'fld', 'flds', 'fls', 'flt', 'flts', 'ford',
    'fords', 'forest', 'forests', 'forg', 'forge', 'forges', 'fork', 'forks',
    'fort', 'frd', 'frds', 'freeway', 'freewy', 'frg', 'frgs', 'frk', 'frks',
    'frry', 'frst', 'frt', 'frway', 'frwy', 'fry', 'ft', 'fwy', 'garden',
    'gardens', 'gardn', 'gateway', 'gatewy', 'gatway', 'gdn', 'gdns', 'glen',
    'glens', 'gln', 'glns', 'grden', 'grdn', 'grdns', 'green', 'greens', 'grn',
    'grns', 'grov', 'grove', 'groves', 'grv', 'grvs', 'gtway', 'gtwy', 'harb',
    'harbor', 'harbors', 'harbr', 'haven', 'havn', 'hbr', 'hbrs', 'height',
    'heights', 'hgts', 'highway', 'highwy', 'hill', 'hills', 'hiway', 'hiwy',
    'hl', 'hllw', 'hls', 'hollow', 'hollows', 'holw', 'holws', 'hrbor', 'ht',
    'hts', 'hvn', 'hway', 'hwy', 'inlet', 'inlt', 'is', 'island', 'islands',
    'isle', 'isles', 'islnd', 'islnds', 'iss', 'jct', 'jction', 'jctn',
    'jctns', 'jcts', 'junction', 'junctions', 'junctn', 'juncton', 'key',
    'keys', 'knl', 'knls', 'knol', 'knoll', 'knolls', 'ky', 'kys', 'la',
    'lake', 'lakes', 'land', 'landing', 'lane', 'lanes', 'lck', 'lcks', 'ldg',
    'ldge', 'lf', 'lgt', 'lgts', 'light', 'lights', 'lk', 'lks', 'ln', 'lndg',
    'lndng', 'loaf', 'lock', 'locks', 'lodg', 'lodge', 'loop', 'loops', 'lp',
    'mall', 'manor', 'manors', 'mdw', 'mdws', 'meadow', 'meadows', 'medows',
    'mews', 'mi', 'mile', 'mill', 'mills', 'mission', 'missn', 'ml', 'mls',
    'mn', 'mnr', 'mnrs', 'mnt', 'mntain', 'mntn', 'mntns', 'motorway', 'mount',
    'mountain', 'mountains', 'mountin', 'msn', 'mssn', 'mt', 'mtin', 'mtn',
    'mtns', 'mtwy', 'nck', 'neck', 'opas', 'orch', 'orchard', 'orchrd', 'oval',
    'overlook', 'overpass', 'ovl', 'ovlk', 'park', 'parks', 'parkway',
    'parkways', 'parkwy', 'pass', 'passage', 'path', 'paths', 'pike', 'pikes',
    'pine', 'pines', 'pk', 'pkway', 'pkwy', 'pkwys', 'pky', 'pl', 'place',
    'plain', 'plaines', 'plains', 'plaza', 'pln', 'plns', 'plz', 'plza', 'pne',
    'pnes', 'point', 'points', 'port', 'ports', 'pr', 'prairie', 'prarie',
    'prk', 'prr', 'prt', 'prts', 'psge', 'pt', 'pts', 'pw', 'pwy', 'rad',
    'radial', 'radiel', 'radl', 'ramp', 'ranch', 'ranches', 'rapid', 'rapids',
    'rd', 'rdg', 'rdge', 'rdgs', 'rds', 'rest', 'ri', 'ridge', 'ridges',
    'rise', 'riv', 'river', 'rivr', 'rn', 'rnch', 'rnchs', 'road', 'roads',
    'route', 'row', 'rpd', 'rpds', 'rst', 'rte', 'rue', 'run', 'rvr', 'shl',
    'shls', 'shoal', 'shoals', 'shoar', 'shoars', 'shore', 'shores', 'shr',
    'shrs', 'skwy', 'skyway', 'smt', 'spg', 'spgs', 'spng', 'spngs', 'spring',
    'springs', 'sprng', 'sprngs', 'spur', 'spurs', 'sq', 'sqr', 'sqre', 'sqrs',
    'sqs', 'squ', 'square', 'squares', 'st', 'sta', 'station', 'statn', 'stn',
    'str', 'stra', 'strav', 'strave', 'straven', 'stravenue', 'stravn',
    'stream', 'street', 'streets', 'streme', 'strm', 'strt', 'strvn',
    'strvnue', 'sts', 'sumit', 'sumitt', 'summit', 'te', 'ter', 'terr',
    'terrace', 'throughway', 'tl', 'tpk', 'tpke', 'tr', 'trace', 'traces',
    'track', 'tracks', 'trafficway', 'trail', 'trailer', 'trails', 'trak',
    'trce', 'trfy', 'trk', 'trks', 'trl', 'trlr', 'trlrs', 'trls', 'trnpk',
    'trpk', 'trwy', 'tunel', 'tunl', 'tunls', 'tunnel', 'tunnels', 'tunnl',
    'turn', 'turnpike', 'turnpk', 'un', 'underpass', 'union', 'unions', 'uns',
    'upas', 'valley', 'valleys', 'vally', 'vdct', 'via', 'viadct', 'viaduct',
    'view', 'views', 'vill', 'villag', 'village', 'villages', 'ville', 'villg',
    'villiage', 'vis', 'vist', 'vista', 'vl', 'vlg', 'vlgs', 'vlly', 'vly',
    'vlys', 'vst', 'vsta', 'vw', 'vws', 'walk', 'walks', 'wall', 'way', 'ways',
    'well', 'wells', 'wl', 'wls', 'wy', 'xc', 'xg', 'xing', 'xrd', 'xrds'
}


def tokenFeatures(token):
    """Extract features for a single token.
    
    Feature names are designed to match the C feature_extractor.c format.
    """
    if token in ('&', '#', '½'):
        token_clean = token
    else:
        token_clean = re.sub(r'(^[\W]*|[\W]*$)', '', token)
    
    token_abbrev = re.sub(r'[.]', '', token_clean.lower())
    is_digit = token_abbrev.isdigit()
    
    features = []
    
    # Word feature (matches C: word=%s)
    if not is_digit and token_abbrev:
        features.append(f'word={token_abbrev}')
    elif is_digit:
        features.append(f'word={token_abbrev}')
    
    # Casing features (matches C format)
    if token_clean.isupper() and token_clean.isalpha():
        features.append('word.isupper')
    if token_clean and token_clean[0].isupper():
        features.append('word.istitle')
    if any(c.isdigit() for c in token_clean):
        features.append('word.hasdigit')
    if is_digit:
        features.append('word.isdigit')
    
    # Direction (matches C: word.isdirection)
    if token_abbrev in DIRECTIONS:
        features.append('word.isdirection')
    
    # Ends with period (matches C: word.endswithperiod)
    if token_clean and token_clean.endswith('.'):
        features.append('word.endswithperiod')
    
    return features



def tokens2features(tokens):
    """Convert a list of tokens to a list of feature dictionaries.
    
    Feature names are designed to match the C feature_extractor.c format:
    - BOS for beginning of string
    - EOS for end of string
    - prev_word=X for previous word
    - next_word=X for next word
    """
    feature_sequence = []
    
    for i, token in enumerate(tokens):
        features = tokenFeatures(token)
        feature_dict = {}
        
        # Current token features (already in correct format from tokenFeatures)
        for feat in features:
            feature_dict[feat] = 1.0
        
        # Previous token context (matches C: prev_word=%s or BOS)
        if i > 0:
            # Add prev_word feature
            prev_token = tokens[i-1]
            prev_clean = re.sub(r'(^[\W]*|[\W]*$)', '', prev_token)
            prev_abbrev = re.sub(r'[.]', '', prev_clean.lower())
            if prev_abbrev:
                feature_dict[f'prev_word={prev_abbrev}'] = 1.0
        else:
            # Beginning of string (matches C: BOS)
            feature_dict['BOS'] = 1.0
        
        # Next token context (matches C: next_word=%s or EOS)
        if i < len(tokens) - 1:
            # Add next_word feature
            next_token = tokens[i+1]
            next_clean = re.sub(r'(^[\W]*|[\W]*$)', '', next_token)
            next_abbrev = re.sub(r'[.]', '', next_clean.lower())
            if next_abbrev:
                feature_dict[f'next_word={next_abbrev}'] = 1.0
        else:
            # End of string (matches C: EOS)
            feature_dict['EOS'] = 1.0
        
        feature_sequence.append(feature_dict)
    
    return feature_sequence


def parse_xml_file(filepath):
    """Parse an XML file and extract (tokens, labels) pairs."""
    sequences = []
    
    try:
        tree = ET.parse(filepath)
        root = tree.getroot()
    except ET.ParseError as e:
        print(f"  Warning: Could not parse {filepath}: {e}")
        return sequences
    
    # Find all AddressString elements
    for addr_elem in root.iter('AddressString'):
        tokens = []
        labels = []
        
        for child in addr_elem:
            label = child.tag
            text = child.text or ''
            # Split text into tokens
            for token in text.split():
                token = token.strip()
                if token:
                    tokens.append(token)
                    labels.append(label)
        
        if tokens:
            sequences.append((tokens, labels))
    
    return sequences


def main():
    # Directories to search for training data
    training_dirs = ['training_data']
    
    all_sequences = []
    
    for training_dir in training_dirs:
        if not os.path.isdir(training_dir):
            print(f"Skipping {training_dir} (not found)")
            continue
        
        print(f"Processing {training_dir}/...")
        
        # Find all XML files (with or without .bak extension)
        xml_files = glob.glob(os.path.join(training_dir, '*.xml'))
        
        for xml_file in xml_files:
            print(f"  Reading {xml_file}...")
            sequences = parse_xml_file(xml_file)
            print(f"    Found {len(sequences)} address sequences")
            all_sequences.extend(sequences)
    
    print(f"\nTotal sequences: {len(all_sequences)}")
    
    if not all_sequences:
        print("Error: No training data found!")
        return 1
    
    # Convert to CRF features
    X_train = []
    y_train = []
    
    for tokens, labels in all_sequences:
        features = tokens2features(tokens)
        X_train.append(features)
        y_train.append(labels)
    
    # Train the model
    print("\nTraining CRF model...")
    trainer = pycrfsuite.Trainer(verbose=True)
    
    for xseq, yseq in zip(X_train, y_train):
        trainer.append(xseq, yseq)
    
    # Set training parameters
    trainer.set_params({
        'c1': 0.0,           # L1 regularization
        'c2': 0.001,         # L2 regularization
        'max_iterations': 100,
        'feature.possible_transitions': True,
        'feature.minfreq': 0,
    })
    
    # Train and save the model
    output_path = 'include/usaddr.crfsuite'
    os.makedirs('include', exist_ok=True)
    
    trainer.train(output_path)
    
    print(f"\nModel saved to {output_path}")
    print(f"Model size: {os.path.getsize(output_path)} bytes")
    
    return 0


if __name__ == '__main__':
    exit(main())
