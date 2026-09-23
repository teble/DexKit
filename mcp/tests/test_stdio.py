#!/usr/bin/env python3
"""Exercise the built MCP binary over real subprocess pipes, without a shell."""
import argparse
import json
import os
from pathlib import Path
import queue
import signal
import subprocess
import tempfile
import threading
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / 'mcp/target/debug/dexkit-mcp'
FIXTURES = ROOT / 'tests/interop/planus/target/fixture'
CASES = []
TOOLS = {}


class Client:
    def __init__(self, root, modern=False):
        self.stderr = tempfile.TemporaryFile(mode='w+b')
        environment = os.environ.copy()
        environment['TMPDIR'] = str(root)
        self.process = subprocess.Popen(
            [str(BINARY), '--allow-root', str(root)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=self.stderr, env=environment)
        self.messages = queue.Queue()
        self.ident = 0
        self.modern = modern
        def read():
            for line in self.process.stdout:
                self.messages.put(line)
            self.messages.put(None)
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        if not modern:
            response = self.rpc('initialize', {'protocolVersion': '2025-11-25',
                'capabilities': {}, 'clientInfo': {'name': 'dexkit-test', 'version': '1'}})
            assert response['result']['protocolVersion'] == '2025-11-25', response
            self.send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
        definitions = self.rpc('tools/list', {})['result']['tools']
        for tool in definitions:
            TOOLS[tool['name']] = tool

    def send(self, value):
        self.process.stdin.write((json.dumps(value, ensure_ascii=True) + '\n').encode())
        self.process.stdin.flush()

    def rpc(self, method, params):
        self.ident += 1
        params = dict(params)
        if self.modern:
            params['_meta'] = {
                'io.modelcontextprotocol/protocolVersion': '2026-07-28',
                'io.modelcontextprotocol/clientInfo': {'name': 'dexkit-test', 'version': '1'},
                'io.modelcontextprotocol/clientCapabilities': {}}
        self.send({'jsonrpc': '2.0', 'id': self.ident, 'method': method, 'params': params})
        while True:
            line = self.messages.get(timeout=15)
            assert line is not None, 'MCP exited without a response'
            message = json.loads(line)  # Any native stdout contamination fails here.
            if message.get('id') == self.ident:
                return message

    def call(self, name, arguments, success=True, valid_input=True):
        name = 'dexkit_v1_' + name
        response = self.rpc('tools/call', {'name': name, 'arguments': arguments})
        assert 'result' in response, response
        result = response['result']
        structured = result['structuredContent']
        assert result.get('isError', False) == (not success), result
        assert structured['ok'] == success, structured
        assert json.loads(result['content'][0]['text']) == structured
        CASES.append({'name': name, 'arguments': arguments,
                      'output': structured, 'validInput': valid_input})
        return structured['data'] if success else structured['error']

    def close(self):
        if self.process.stdin and not self.process.stdin.closed:
            self.process.stdin.close()
        try:
            self.process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
            raise AssertionError('MCP did not exit after stdin closed')
        self.reader.join(timeout=2)
        self.process.stdout.close()
        self.stderr.seek(0)
        diagnostic = self.stderr.read().decode(errors='replace')
        self.stderr.close()
        assert self.process.returncode == 0, diagnostic


class StdioTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dexkit-mcp-e2e-')
        self.root = Path(self.directory.name)
        self.dex = self.root / 'fixture with spaces.dex'
        self.dex.write_bytes((FIXTURES / 'fixture.dex').read_bytes())
        self.unicode = self.root / 'unicode.dex'
        self.unicode.write_bytes((FIXTURES / 'unicode.dex').read_bytes())
        self.client = Client(self.root)

    def tearDown(self):
        try:
            self.client.close()
        finally:
            self.directory.cleanup()

    def open(self, path=None):
        return self.client.call('open', {'path': str(path or self.dex)})['instanceId']

    def test_unicode_query_paging_artifact_and_close(self):
        instance = self.open()
        capabilities = self.client.call('capabilities', {})
        self.assertFalse(capabilities['nativeCancellation'])
        self.assertEqual(len(TOOLS), 11)
        query = {'matcher': {'usingStrings': [{'value': 'pair\U0001f600', 'match': 'equal'}]}}
        found = self.client.call('find_methods', {'instanceId': instance, 'query': query, 'pageSize': 1})
        self.assertEqual(found['resultSet']['totalItems'], '1')
        entity = found['items'][0]['entityId']
        inline = self.client.call('smali', {'instanceId': instance, 'entityId': entity})
        self.assertIn('\\ud83d\\ude00', inline['text'])
        artifact = self.client.call('smali', {'instanceId': instance, 'entityId': entity, 'delivery': 'artifact'})['artifact']
        self.assertEqual(set(self.root.iterdir()), {self.dex, self.unicode})
        resource = self.client.rpc('resources/read', {'uri': artifact['uri']})
        self.assertEqual(resource['result']['contents'][0]['text'], inline['text'])
        chunk = self.client.call('read_artifact', {'instanceId': instance, 'artifactId': artifact['id'], 'maxBytes': 32})
        self.assertEqual(chunk['text'], inline['text'][:32])
        all_rows = self.client.call('find_methods', {'instanceId': instance, 'query': {}, 'pageSize': 2})
        second = self.client.call('page', {'instanceId': instance, 'cursor': all_rows['page']['nextCursor']})
        self.assertEqual(second['page']['returned'], 2)
        self.client.call('close', {'instanceId': instance})
        error = self.client.call('page', {'instanceId': instance, 'cursor': all_rows['page']['nextCursor']}, success=False)
        self.assertEqual(error['code'], 'INSTANCE_CLOSED')
        self.assertIn('error', self.client.rpc('resources/read', {'uri': artifact['uri']}))

    def test_metadata_and_valid_parameter_null_schema(self):
        instance = self.open(self.unicode)
        found = self.client.call('find_classes', {'instanceId': instance, 'query': {}})
        entity = found['items'][0]['entityId']
        described = self.client.call('describe', {'instanceId': instance, 'entityId': entity, 'include': ['annotations']})
        self.assertEqual(described['entity']['source']['sourceFile'], 'source\0\U0001f600')
        methods = self.client.call('relations', {'instanceId': instance, 'entityId': entity, 'relation': 'declaredMethods'})
        self.assertEqual(methods['resultSet']['totalItems'], '1')
        another = self.open()
        match = {'parameters': {'parameters': [None, {'parameterType': {'className': {'value': 'java.lang.String', 'match': 'equal'}}}]}}
        result = self.client.call('find_methods', {'instanceId': another, 'query': {'matcher': match}})
        self.assertEqual(result['resultSet']['totalItems'], '1')
        field = self.client.call('find_fields', {'instanceId': another, 'query': {'matcher': {'fieldName': {'value': 'counter', 'match': 'equal'}}}})
        self.assertEqual(field['items'][0]['accessFlags'], 9)

    def test_semantic_and_protocol_errors(self):
        instance = self.open()
        for matcher in [{'unknown': True}, {'anyOf': []}, {'parameters': None}]:
            error = self.client.call('find_methods', {'instanceId': instance, 'query': {'matcher': matcher}}, success=False, valid_input=False)
            self.assertEqual(error['category'], 'invalid_request')
        no_match = self.client.call('find_methods', {'instanceId': instance, 'query': {'scope': {'within': {'entityIds': []}}}})
        self.assertEqual(no_match['resultSet']['totalItems'], '0')
        error = self.client.call('open', {'path': str(Path(__file__).resolve())}, success=False)
        self.assertEqual(error['code'], 'PATH_NOT_ALLOWED')
        malformed = self.root / 'invalid.dex'
        malformed.write_bytes(b'not a dex')
        self.client.call('open', {'path': str(malformed)}, success=False)
        unknown = self.client.rpc('tools/call', {'name': 'dexkit_v1_unknown', 'arguments': {}})
        self.assertIn('error', unknown)
        self.assertIn('error', self.client.rpc('resources/read', {'uri': 'file:///etc/passwd'}))

    def test_zip_multidex_and_gaps(self):
        apk = self.root / 'sample.apk'
        with zipfile.ZipFile(apk, 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('classes.dex', self.dex.read_bytes())
            archive.writestr('classes2.dex', self.unicode.read_bytes())
        opened = self.client.call('open', {'path': str(apk)})
        self.assertEqual(opened['dexCount'], 2)
        classes = self.client.call('find_classes', {'instanceId': opened['instanceId'], 'query': {}})
        self.assertEqual(classes['resultSet']['totalItems'], '2')
        gap = self.root / 'gap.apk'
        with zipfile.ZipFile(gap, 'w') as archive:
            archive.writestr('classes.dex', self.dex.read_bytes())
            archive.writestr('classes3.dex', self.unicode.read_bytes())
        self.client.call('open', {'path': str(gap)}, success=False)

    def test_worker_failure_is_a_tool_error(self):
        instance = self.open()
        entity = self.client.call('find_classes', {'instanceId': instance, 'query': {}})['items'][0]['entityId']
        self.client.call('smali', {'instanceId': instance, 'entityId': entity, 'delivery': 'artifact'})
        # Fault injection targets only the direct child created by this test.
        children = subprocess.check_output(['pgrep', '-P', str(self.client.process.pid)], text=True).split()
        self.assertEqual(len(children), 1)
        os.kill(int(children[0]), signal.SIGKILL)
        error = self.client.call('capabilities', {}, success=False)
        self.assertEqual(error['code'], 'WORKER_EXITED')
        self.assertEqual(len(self.client.rpc('tools/list', {})['result']['tools']), 11)
        self.assertEqual(set(self.root.iterdir()), {self.dex, self.unicode})

    def test_artifact_is_unlinked_on_normal_stdin_close(self):
        instance = self.open()
        entity = self.client.call('find_classes', {'instanceId': instance, 'query': {}})['items'][0]['entityId']
        self.client.call('smali', {'instanceId': instance, 'entityId': entity, 'delivery': 'artifact'})
        self.client.close()
        self.assertEqual(set(self.root.iterdir()), {self.dex, self.unicode})
        self.client = Client(self.root)

    def test_deep_metadata_is_a_limit_error_and_keeps_instance_alive(self):
        path = self.root / 'deep.dex'
        path.write_bytes((FIXTURES / 'deep.dex').read_bytes())
        instance = self.open(path)
        entity = self.client.call('find_classes', {'instanceId': instance, 'query': {}})['items'][0]['entityId']
        error = self.client.call('describe', {'instanceId': instance, 'entityId': entity, 'include': ['annotations']}, success=False)
        self.assertEqual(error['code'], 'LIMIT_EXCEEDED')
        result = self.client.call('find_classes', {'instanceId': instance, 'query': {}})
        self.assertEqual(result['resultSet']['totalItems'], '1')

    def test_reencoded_large_request_does_not_kill_worker(self):
        instance = self.open()
        item = '{"type":"float32","value":1e2}'
        count = (2 * 1024 * 1024 - 1200) // (len(item) + 1)
        self.client.ident += 1
        prefix = ('{"jsonrpc":"2.0","id":' + str(self.client.ident)
                  + ',"method":"tools/call","params":{"name":"dexkit_v1_find_methods",'
                  + '"arguments":{"instanceId":' + json.dumps(instance)
                  + ',"query":{"matcher":{"usingNumbers":[')
        frame = (prefix + ','.join([item] * count) + ']}}}}}\n').encode()
        self.assertLess(len(frame), 2 * 1024 * 1024)
        self.client.process.stdin.write(frame)
        self.client.process.stdin.flush()
        response = json.loads(self.client.messages.get(timeout=15))
        self.assertEqual(response['id'], self.client.ident)
        self.assertEqual(response['result']['structuredContent']['error']['code'], 'LIMIT_EXCEEDED')
        result = self.client.call('find_methods', {'instanceId': instance, 'query': {}})
        self.assertEqual(result['resultSet']['totalItems'], '7')

    def test_cancelled_queued_opens_do_not_consume_instance_slots(self):
        instance = self.open()
        children = subprocess.check_output(['pgrep', '-P', str(self.client.process.pid)], text=True).split()
        self.assertEqual(len(children), 1)
        worker = int(children[0])
        os.kill(worker, signal.SIGSTOP)
        try:
            for _ in range(4):
                self.client.ident += 1
                request_id = self.client.ident
                self.client.send({'jsonrpc': '2.0', 'id': request_id, 'method': 'tools/call',
                                  'params': {'name': 'dexkit_v1_open', 'arguments': {'path': str(self.dex)}}})
                self.client.send({'jsonrpc': '2.0', 'method': 'notifications/cancelled',
                                  'params': {'requestId': request_id, 'reason': 'test cancellation'}})
                # rmcp intentionally suppresses cancelled responses. tools/list
                # is a protocol barrier and must remain available while stopped.
                self.assertEqual(len(self.client.rpc('tools/list', {})['result']['tools']), 11)
        finally:
            os.kill(worker, signal.SIGCONT)
        # At most one cancelled open was already active. Queued opens must be
        # skipped, leaving two of the four instance slots available.
        self.open()
        self.open()
        result = self.client.call('find_methods', {'instanceId': instance, 'query': {}})
        self.assertEqual(result['resultSet']['totalItems'], '7')

    def test_latest_protocol_without_initialize(self):
        modern = Client(self.root, modern=True)
        try:
            response = modern.rpc('tools/call', {'name': 'dexkit_v1_capabilities', 'arguments': {}})
            self.assertEqual(response['result']['resultType'], 'complete')
            self.assertTrue(response['result']['structuredContent']['ok'])
            listing = modern.rpc('tools/list', {})['result']
            self.assertEqual(listing['ttlMs'], 0)
            self.assertEqual(listing['cacheScope'], 'private')
        finally:
            modern.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, default=BINARY)
    parser.add_argument('--captures', type=Path, default=ROOT / 'mcp/target/contract-cases.json')
    options, rest = parser.parse_known_args()
    BINARY = options.binary.resolve()
    result = unittest.main(argv=[__file__] + rest, exit=False)
    options.captures.parent.mkdir(parents=True, exist_ok=True)
    options.captures.write_text(json.dumps({'tools': list(TOOLS.values()), 'cases': CASES}, ensure_ascii=True))
    raise SystemExit(0 if result.result.wasSuccessful() else 1)
