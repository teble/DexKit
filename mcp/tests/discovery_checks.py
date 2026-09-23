"""Shared real-transport checks for on-demand query contracts."""
import hashlib
import json


def exercise(client, fixture, tools):
    capabilities = client.call('capabilities', {})
    assert set(capabilities['tools']) == set(tools)
    assert capabilities['querySchemaDiscovery']['requiresInstance'] is False
    assert capabilities['querySchemaDiscovery']['execution'] == 'parent'
    overviews = []
    for suffix in ('classes', 'methods', 'fields'):
        name = 'dexkit_v1_find_' + suffix
        overview = client.call('get_query_schema', {'tool': name})
        assert overview['kind'] == 'overview'
        assert 'fragment' not in overview
        full = client.call('get_query_schema', overview['readFull'])
        schema = tools[name]['inputSchema']
        assert full['fragment'] == schema
        canonical = json.dumps(schema, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()
        assert full['schemaHash'] == 'sha256:' + hashlib.sha256(canonical).hexdigest()
        assert schema['properties']['select']['items']['enum'] == ['descriptor', 'flags', 'source']
        matcher = next(link for link in overview['links'] if link.get('label') == 'matcher')
        fragment = client.call('get_query_schema', {'tool': name, 'pointer': matcher['pointer'],
                                                   'ifSchemaHash': overview['schemaHash']})
        target = next(link for link in fragment['links'] if link['relation'] == '$ref')
        matcher_doc = client.call('get_query_schema', {'tool': name, 'pointer': target['pointer']})
        assert 'properties' in matcher_doc['fragment']
        assert matcher_doc['standalone'] is False
        overviews.append((name, overview))
    for arguments, code, valid in [
        ({'tool': 'dexkit_v1_find_methods', 'pointer': '/missing'}, 'SCHEMA_POINTER_INVALID', True),
        ({'tool': 'dexkit_v1_find_methods', 'pointer': '#/properties/query'}, 'SCHEMA_POINTER_INVALID', True),
        ({'tool': 'dexkit_v1_find_methods', 'ifSchemaHash': 'old'}, 'SCHEMA_CHANGED', True),
        ({'tool': 'dexkit_v1_find_methods', 'pointer': None}, 'INVALID_ARGUMENT', False),
        ({'tool': 'dexkit_v1_find_methods', 'extra': True}, 'INVALID_ARGUMENT', False),
        ({'tool': 'dexkit_v1_open'}, 'INVALID_ARGUMENT', False),
    ]:
        error = client.call('get_query_schema', arguments, success=False, valid_input=valid)
        assert error['code'] == code, error
    instance = client.call('open', {'path': str(fixture)})['instanceId']
    try:
        for name, overview in overviews:
            for example in overview['examples']:
                arguments = {**example, 'instanceId': instance}
                result = client.call(name.removeprefix('dexkit_v1_'), arguments)
                assert result['resultSet']['totalItems'] == '1', result
    finally:
        client.call('close', {'instanceId': instance})
