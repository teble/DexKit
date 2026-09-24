#!/usr/bin/env python3
"""Exercise the built MCP binary over real subprocess pipes, without a shell."""
import argparse
import json
import os
from pathlib import Path
import queue
import signal
import struct
import subprocess
import tempfile
import threading
import unittest
import zipfile
from discovery_checks import exercise as exercise_discovery

ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / 'mcp/target/debug/dexkit-mcp'
FIXTURES = ROOT / 'tests/interop/planus/target/fixture'
CASES = []
TOOLS = {}


class Client:
    def __init__(self, root, modern=False, extra_args=()):
        self.stderr = tempfile.TemporaryFile(mode='w+b')
        environment = os.environ.copy()
        environment['TMPDIR'] = str(root)
        self.process = subprocess.Popen(
            [str(BINARY), '--allow-root', str(root), *extra_args], stdin=subprocess.PIPE,
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
        name = 'dexkit_' + name
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

    def test_query_discovery_without_docs_and_examples(self):
        exercise_discovery(self.client, self.dex, TOOLS)

    def test_unicode_query_paging_artifact_and_close(self):
        instance = self.open()
        capabilities = self.client.call('capabilities', {})
        self.assertFalse(capabilities['nativeCancellation'])
        self.assertEqual(len(TOOLS), 12)
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
        unknown = self.client.rpc('tools/call', {'name': 'dexkit_unknown', 'arguments': {}})
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

    def test_thread_options_and_parallel_multidex(self):
        automatic = self.client.call('capabilities', {})['nativeThreads']
        self.assertGreaterEqual(automatic, 1)
        expected = None
        for threads in [0, 1, 3]:
            with self.subTest(threads=threads):
                apk = self.root / 'mixed.apk'
                with zipfile.ZipFile(apk, 'w') as archive:
                    archive.writestr('classes.dex', self.dex.read_bytes(), compress_type=zipfile.ZIP_STORED)
                    archive.writestr('classes2.dex', self.unicode.read_bytes(), compress_type=zipfile.ZIP_DEFLATED)
                    archive.writestr('classes3.dex', self.dex.read_bytes(), compress_type=zipfile.ZIP_DEFLATED)
                client = Client(self.root, extra_args=['--threads', str(threads)])
                try:
                    self.assertEqual(client.call('capabilities', {})['nativeThreads'], threads or automatic)
                    opened = client.call('open', {'path': str(apk)})
                    self.assertEqual(opened['dexCount'], 3)
                    instance = opened['instanceId']
                    # Both stored and deflated images must outlive this input.
                    apk.write_bytes(b'input replaced after open')
                    found = client.call('find_classes', {'instanceId': instance, 'query': {}})
                    rows = [(row['descriptor'], row['source']['dexIndex']) for row in found['items']]
                    self.assertEqual(sorted(index for _, index in rows), [0, 1, 2])
                    if expected is None:
                        expected = rows
                    self.assertEqual(rows, expected)
                    exact = client.call('find_classes', {'instanceId': instance, 'query': {
                        'matcher': {'className': {'value': 'interop.Probe', 'match': 'equal'}}}})
                    self.assertEqual(exact['resultSet']['totalItems'], '1')
                    self.assertEqual(exact['items'][0]['source']['dexIndex'], 2)
                    for row in found['items']:
                        smali = client.call('smali', {'instanceId': instance, 'entityId': row['entityId']})
                        self.assertIn(row['descriptor'], smali['text'])
                    client.call('close', {'instanceId': instance})
                    # A bad later entry must fail the complete load, while the
                    # worker remains usable for a subsequent valid request.
                    with zipfile.ZipFile(apk, 'w', zipfile.ZIP_DEFLATED) as archive:
                        archive.writestr('classes.dex', self.dex.read_bytes())
                        archive.writestr('classes2.dex', b'not a DEX')
                    client.call('open', {'path': str(apk)}, success=False)
                    recovered = client.call('open', {'path': str(self.dex)})['instanceId']
                    client.call('close', {'instanceId': recovered})
                finally:
                    client.close()

    def test_large_resource_apk_and_snapshot_lifetime(self):
        apk = self.root / 'resource-heavy.apk'
        with zipfile.ZipFile(apk, 'w') as archive:
            # Keep the fixture generation bounded too: resources exceed the old
            # container ceiling, but the two DEX entries remain small.
            with archive.open('assets/padding.dat', 'w') as asset:
                chunk = bytes(1024 * 1024)
                for _ in range(257):
                    asset.write(chunk)
            for name, path in [('classes.dex', self.dex), ('classes2.dex', self.unicode)]:
                info = zipfile.ZipInfo(name)
                padding = -(archive.fp.tell() + 30 + len(name) + 4) % 4
                info.extra = struct.pack('<HH', 0xcafe, padding) + bytes(padding)
                archive.writestr(info, path.read_bytes())
                # Stored/aligned entries must be detached from the APK mapping.
                self.assertEqual((info.header_offset + 30 + len(name) + len(info.extra)) % 4, 0)
        self.assertGreater(apk.stat().st_size, 256 * 1024 * 1024)
        opened = self.client.call('open', {'path': str(apk)})
        self.assertEqual(set(opened), {'instanceId', 'byteLength', 'dexCount'})
        self.assertEqual(opened['dexCount'], 2)
        self.assertEqual(opened['byteLength'], str(apk.stat().st_size))
        apk.write_bytes(b'replaced and truncated')
        apk.unlink()
        instance = opened['instanceId']
        found = self.client.call('find_classes', {'instanceId': instance, 'query': {}})
        self.assertEqual(found['resultSet']['totalItems'], '2')
        self.client.call('smali', {'instanceId': instance, 'entityId': found['items'][0]['entityId']})
        self.client.call('close', {'instanceId': instance})
        self.assertEqual(set(self.root.iterdir()), {self.dex, self.unicode})

    def test_configurable_input_and_dex_budgets(self):
        capabilities = self.client.call('capabilities', {})
        self.assertEqual(capabilities['maxInputBytes'], 0)
        self.assertEqual(capabilities['maxDexBytes'], 512 * 1024 * 1024)
        raw = self.root / 'padded.dex'
        raw.write_bytes(self.dex.read_bytes().ljust(1024 * 1024 + 1, b'\0'))
        apk = self.root / 'aggregate.apk'
        with zipfile.ZipFile(apk, 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('classes.dex', self.dex.read_bytes().ljust(600 * 1024, b'\0'))
            archive.writestr('classes2.dex', self.unicode.read_bytes().ljust(600 * 1024, b'\0'))
        cases = [(['--max-input-mib', '1'], raw, '--max-input-mib'),
                 (['--max-dex-mib', '1'], raw, '--max-dex-mib'),
                 (['--max-dex-mib', '1'], apk, '--max-dex-mib')]
        for args, path, flag in cases:
            client = Client(self.root, extra_args=args)
            try:
                for _ in range(5):  # Failed opens must not consume instance slots.
                    error = client.call('open', {'path': str(path)}, success=False)
                    self.assertEqual(error['code'], 'LIMIT_EXCEEDED')
                    self.assertIn(flag, error['message'])
                client.call('open', {'path': str(self.dex)})
            finally:
                client.close()
        for limit in ['2', '0']:
            client = Client(self.root, extra_args=['--max-input-mib', '0', '--max-dex-mib', limit])
            try:
                self.assertEqual(client.call('capabilities', {})['maxDexBytes'], int(limit) * 1024 * 1024)
                for path, count in [(raw, 1), (apk, 2)]:
                    opened = client.call('open', {'path': str(path)})
                    self.assertEqual(opened['dexCount'], count)
                    client.call('close', {'instanceId': opened['instanceId']})
            finally:
                client.close()

    def test_dex_budget_preflight_before_loading_and_no_hidden_ceiling(self):
        apk = self.root / 'preflight.apk'
        with zipfile.ZipFile(apk, 'w') as archive:
            archive.writestr('classes.dex', b'invalid first DEX')
            archive.writestr('classes2.dex', b'not compressed')
        data = bytearray(apk.read_bytes())
        second = data.index(b'PK\x01\x02', data.index(b'PK\x01\x02') + 1)
        struct.pack_into('<I', data, second + 24, 512 * 1024 * 1024 + 1)
        struct.pack_into('<H', data, second + 10, 99)  # Unsupported method; never inflate a huge fixture.
        apk.write_bytes(data)
        # The complete aggregate is checked before even parsing the first DEX.
        error = self.client.call('open', {'path': str(apk)}, success=False)
        self.assertEqual(error['code'], 'LIMIT_EXCEEDED')
        for limit in ['1024', '0']:
            client = Client(self.root, extra_args=['--max-dex-mib', limit])
            try:
                error = client.call('open', {'path': str(apk)}, success=False)
                self.assertEqual(error['code'], 'INVALID_INPUT')
                client.call('open', {'path': str(self.dex)})
            finally:
                client.close()

    def test_startup_option_validation(self):
        for flag in ['--max-input-mib', '--max-dex-mib', '--threads']:
            for value in ['-1', '1.5', 'no', '4294967296', '18446744073709551616']:
                result = subprocess.run([str(BINARY), flag, value], capture_output=True, timeout=5)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(flag.encode(), result.stderr)
            result = subprocess.run([str(BINARY), flag], capture_output=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
        for value in ['2147483648', '+2', '']:
            result = subprocess.run([str(BINARY), '--threads', value], capture_output=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'--threads', result.stderr)

    def test_worker_failure_is_a_tool_error(self):
        instance = self.open()
        entity = self.client.call('find_classes', {'instanceId': instance, 'query': {}})['items'][0]['entityId']
        self.client.call('smali', {'instanceId': instance, 'entityId': entity, 'delivery': 'artifact'})
        # Fault injection targets only the direct child created by this test.
        children = subprocess.check_output(['pgrep', '-P', str(self.client.process.pid)], text=True).split()
        self.assertEqual(len(children), 1)
        os.kill(int(children[0]), signal.SIGKILL)
        self.assertEqual(self.client.call('get_query_schema', {'tool': 'dexkit_find_methods'})['kind'], 'overview')
        error = self.client.call('capabilities', {}, success=False)
        self.assertEqual(error['code'], 'WORKER_EXITED')
        self.assertEqual(len(self.client.rpc('tools/list', {})['result']['tools']), 12)
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
                  + ',"method":"tools/call","params":{"name":"dexkit_find_methods",'
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
            self.assertEqual(self.client.call('get_query_schema', {'tool': 'dexkit_find_methods'})['kind'], 'overview')
            for _ in range(4):
                self.client.ident += 1
                request_id = self.client.ident
                self.client.send({'jsonrpc': '2.0', 'id': request_id, 'method': 'tools/call',
                                  'params': {'name': 'dexkit_open', 'arguments': {'path': str(self.dex)}}})
                self.client.send({'jsonrpc': '2.0', 'method': 'notifications/cancelled',
                                  'params': {'requestId': request_id, 'reason': 'test cancellation'}})
                # rmcp intentionally suppresses cancelled responses. tools/list
                # is a protocol barrier and must remain available while stopped.
                self.assertEqual(len(self.client.rpc('tools/list', {})['result']['tools']), 12)
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
            response = modern.rpc('tools/call', {'name': 'dexkit_capabilities', 'arguments': {}})
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
