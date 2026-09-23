#!/usr/bin/env python3
"""Run the actual executable over loopback HTTP, including SDK SSE framing."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import http.client
import json
import os
from pathlib import Path
import queue
import signal
import socket
import subprocess
import tempfile
import threading
import time
import unittest
from urllib.parse import urlsplit

import test_stdio as common
from discovery_checks import exercise as exercise_discovery

BINARY = common.BINARY
SDK_CLIENT = common.ROOT / 'mcp/target/debug/examples/http_smoke'


class HttpProcess:
    def __init__(self, root):
        environment = os.environ.copy()
        environment['TMPDIR'] = str(root)
        self.process = subprocess.Popen(
            [str(BINARY), '--transport', 'http', '--listen', '127.0.0.1:0',
             '--allow-root', str(root)], stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment)
        self.lines = []
        self.ready = queue.Queue()
        def read():
            for line in self.process.stderr:
                line = line.decode(errors='replace')
                self.lines.append(line)
                if line.startswith('dexkit-mcp listening on '):
                    self.ready.put(line.split(' on ', 1)[1].strip())
            self.ready.put(None)
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()
        self.url = self.ready.get(timeout=15)
        assert self.url, ''.join(self.lines)

    def worker(self):
        children = subprocess.check_output(['pgrep', '-P', str(self.process.pid)], text=True).split()
        assert len(children) == 1, children
        return int(children[0])

    def close(self, sig=signal.SIGTERM):
        if self.process.stdout.closed:
            return
        if self.process.poll() is None:
            self.process.send_signal(sig)
        try:
            self.process.wait(timeout=6)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
            raise AssertionError('HTTP server did not exit within shutdown budget')
        self.reader.join(timeout=2)
        output = self.process.stdout.read()
        self.process.stdout.close()
        self.process.stderr.close()
        assert not output, output
        assert self.process.returncode == 0, ''.join(self.lines)


class HttpClient:
    call = common.Client.call

    def __init__(self, url, modern=True, timeout=8):
        self.url = urlsplit(url)
        self.modern = modern
        self.ident = 0
        self.timeout = timeout

    def connection(self):
        return http.client.HTTPConnection(self.url.hostname, self.url.port, timeout=self.timeout)

    def envelope(self, method, params):
        self.ident += 1
        params = dict(params)
        headers = {'Content-Type': 'application/json',
                   'Accept': 'application/json, text/event-stream',
                   'MCP-Protocol-Version': '2026-07-28' if self.modern else '2025-11-25'}
        if self.modern:
            params['_meta'] = {
                'io.modelcontextprotocol/protocolVersion': '2026-07-28',
                'io.modelcontextprotocol/clientInfo': {'name': 'dexkit-http-test', 'version': '1'},
                'io.modelcontextprotocol/clientCapabilities': {}}
            headers['Mcp-Method'] = method
            if method in ('tools/call', 'resources/read'):
                headers['Mcp-Name'] = params.get('name', params.get('uri'))
        return {'jsonrpc': '2.0', 'id': self.ident, 'method': method, 'params': params}, headers

    def raw(self, body, headers, method='POST', path=None, chunked=False):
        connection = self.connection()
        try:
            connection.request(method, path or self.url.path, body=body, headers=headers,
                               encode_chunked=chunked)
            response = connection.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            connection.close()

    def rpc(self, method, params):
        body, headers = self.envelope(method, params)
        status, response_headers, response = self.raw(json.dumps(body).encode(), headers)
        assert status == 200, (status, response)
        assert not any(k.lower() == 'mcp-session-id' for k in response_headers), response_headers
        content_type = next(v for k, v in response_headers.items() if k.lower() == 'content-type')
        if content_type.startswith('application/json'):
            messages = [json.loads(response)]
        else:
            assert content_type.startswith('text/event-stream'), response_headers
            messages = [json.loads(line[5:].strip()) for line in response.splitlines()
                        if line.startswith(b'data:') and line[5:].strip()]
        return next(message for message in messages if message.get('id') == body['id'])

    def catalogue(self):
        for tool in self.rpc('tools/list', {})['result']['tools']:
            common.TOOLS[tool['name']] = tool


class HttpTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='dexkit-http-')
        self.root = Path(self.directory.name)
        self.dex = self.root / 'unicode with spaces.dex'
        self.dex.write_bytes((common.FIXTURES / 'unicode.dex').read_bytes())
        self.server = HttpProcess(self.root)
        self.client = HttpClient(self.server.url)
        self.client.catalogue()

    def tearDown(self):
        try:
            self.server.close()
        finally:
            self.directory.cleanup()

    def open(self):
        return self.client.call('open', {'path': str(self.dex)})['instanceId']

    def test_query_discovery_without_docs_and_examples(self):
        fixture = self.root / 'query-examples.dex'
        fixture.write_bytes((common.FIXTURES / 'fixture.dex').read_bytes())
        exercise_discovery(self.client, fixture, common.TOOLS)

    def test_handles_survive_new_connections_and_client_discovery(self):
        instance = self.open()
        another = HttpClient(self.server.url)
        another.catalogue()
        found = another.call('find_methods', {'instanceId': instance, 'query': {
            'matcher': {'usingStrings': [{'value': 'pair\U0001f600', 'match': 'equal'}]}}})
        self.assertEqual(found['resultSet']['totalItems'], '1')
        entity = found['items'][0]['entityId']
        described = another.call('describe', {'instanceId': instance, 'entityId': entity,
                                               'include': ['usingStrings']})
        self.assertIn('nul\0end', described['usingStrings'])
        inline = another.call('smali', {'instanceId': instance, 'entityId': entity})
        artifact = another.call('smali', {'instanceId': instance, 'entityId': entity,
                                          'delivery': 'artifact'})['artifact']
        contents = another.rpc('resources/read', {'uri': artifact['uri']})['result']['contents']
        self.assertEqual(contents[0]['text'], inline['text'])
        chunk = another.call('read_artifact', {'instanceId': instance, 'artifactId': artifact['id'],
                                              'maxBytes': 20})
        self.assertEqual(chunk['text'], inline['text'][:20])
        self.client.call('close', {'instanceId': instance})
        error = another.call('describe', {'instanceId': instance, 'entityId': entity}, success=False)
        self.assertEqual(error['code'], 'INSTANCE_CLOSED')

    def test_legacy_initialize_without_protocol_session(self):
        client = HttpClient(self.server.url, modern=False)
        reply = client.rpc('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {},
                                          'clientInfo': {'name': 'legacy-test', 'version': '1'}})
        self.assertEqual(reply['result']['protocolVersion'], '2025-11-25')
        notification = {'jsonrpc': '2.0', 'method': 'notifications/initialized'}
        status, _, _ = client.raw(json.dumps(notification).encode(), {
            'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream',
            'MCP-Protocol-Version': '2025-11-25'})
        self.assertEqual(status, 202)
        client.catalogue()
        instance = client.call('open', {'path': str(self.dex)})['instanceId']
        found = client.call('find_classes', {'instanceId': instance, 'query': {}})
        self.assertEqual(found['resultSet']['totalItems'], '1')
        client.call('close', {'instanceId': instance})

    def test_http_validation_and_body_budgets(self):
        body, headers = self.client.envelope('tools/list', {})
        data = json.dumps(body).encode()
        for extra, expected in [({'Host': 'example.invalid'}, 403),
                                ({'Host': '127.0.0.1:1'}, 403),
                                ({'Origin': 'https://example.invalid'}, 403),
                                ({'Origin': 'null'}, 403),
                                ({'Accept': 'text/plain'}, 406),
                                ({'Content-Type': 'text/plain'}, 415),
                                ({'Mcp-Method': 'tools/call'}, 400)]:
            status, _, _ = self.client.raw(data, {**headers, **extra})
            self.assertEqual(status, expected, extra)
        status, _, _ = self.client.raw(data, {**headers, 'Origin': f'http://127.0.0.1:{self.client.url.port}'})
        self.assertEqual(status, 200)
        status, _, _ = self.client.raw(b'{' + b' ' * (2 * 1024 * 1024), headers)
        self.assertEqual(status, 413)
        chunks = iter([b' ' * 65536] * 33)
        status, _, _ = self.client.raw(chunks, headers, chunked=True)
        self.assertEqual(status, 413)
        for method in ('GET', 'DELETE'):
            status, _, _ = self.client.raw(None, {}, method=method)
            self.assertEqual(status, 405)
        self.assertEqual(self.client.raw(data, headers, path='/unknown')[0], 404)
        self.client.call('capabilities', {})

    def test_concurrent_calls_keep_instances_distinct(self):
        first = self.open()
        second = self.open()
        def query(instance):
            return HttpClient(self.server.url).call('find_classes', {'instanceId': instance, 'query': {}})
        with ThreadPoolExecutor(max_workers=6) as pool:
            results = list(pool.map(query, [first, second] * 3))
        first_id = results[0]['items'][0]['entityId']
        self.assertNotEqual(first_id, results[1]['items'][0]['entityId'])
        self.assertEqual(first_id, results[2]['items'][0]['entityId'])
        error = self.client.call('describe', {'instanceId': second, 'entityId': first_id}, success=False)
        self.assertEqual(error['code'], 'INVALID_ENTITY')

    def test_paging_field_queries_and_relations_over_http(self):
        path = self.root / 'methods.dex'
        path.write_bytes((common.FIXTURES / 'fixture.dex').read_bytes())
        instance = self.client.call('open', {'path': str(path)})['instanceId']
        first = self.client.call('find_methods', {'instanceId': instance, 'query': {}, 'pageSize': 2})
        self.assertEqual(first['resultSet']['totalItems'], '7')
        page = self.client.call('page', {'instanceId': instance, 'cursor': first['page']['nextCursor']})
        self.assertEqual(page['page']['returned'], 2)
        fields = self.client.call('find_fields', {'instanceId': instance, 'query': {}})
        self.assertTrue(fields['items'])
        related = self.client.call('relations', {'instanceId': instance,
            'entityId': fields['items'][0]['entityId'], 'relation': 'fieldReaders'})
        self.assertIsInstance(related['items'], list)
        self.client.call('close', {'instanceId': instance})

    def test_worker_failure_remains_explicit_over_http(self):
        self.open()
        os.kill(self.server.worker(), signal.SIGKILL)
        self.assertEqual(self.client.call('get_query_schema', {'tool': 'dexkit_v1_find_methods'})['kind'], 'overview')
        error = self.client.call('capabilities', {}, success=False)
        self.assertEqual(error['code'], 'WORKER_EXITED')
        self.client.catalogue()

    def test_disconnect_cancels_queued_work_without_losing_instances(self):
        instance = self.open()
        worker = self.server.worker()
        os.kill(worker, signal.SIGSTOP)
        try:
            self.assertEqual(self.client.call('get_query_schema', {'tool': 'dexkit_v1_find_methods'})['kind'], 'overview')
            for _ in range(4):
                connection = self.client.connection()
                body, headers = self.client.envelope('tools/call', {
                    'name': 'dexkit_v1_open', 'arguments': {'path': str(self.dex)}})
                connection.request('POST', '/mcp', json.dumps(body).encode(), headers)
                # Modern rmcp waits for its first message before choosing the
                # HTTP status. Cancel while headers are still pending.
                self.client.catalogue()
                connection.sock.shutdown(socket.SHUT_RDWR)
                connection.close()
                self.client.catalogue()
        finally:
            os.kill(worker, signal.SIGCONT)
        self.open()
        self.open()
        found = self.client.call('find_classes', {'instanceId': instance, 'query': {}})
        self.assertEqual(found['resultSet']['totalItems'], '1')

    def test_partial_bodies_have_a_global_capacity_limit(self):
        connections = []
        body, headers = self.client.envelope('tools/list', {})
        try:
            for _ in range(16):
                connection = self.client.connection()
                connection.putrequest('POST', '/mcp')
                connection.putheader('Content-Type', 'application/json')
                connection.putheader('Accept', 'application/json, text/event-stream')
                connection.putheader('Content-Length', '10000')
                connection.endheaders()
                connection.send(b'{')
                connections.append(connection)
            deadline = time.monotonic() + 3
            while True:
                status, result_headers, _ = self.client.raw(json.dumps(body).encode(), headers)
                if status == 503 or time.monotonic() >= deadline:
                    break
                time.sleep(0.02)
            self.assertEqual(status, 503)
            self.assertEqual(next(v for k, v in result_headers.items() if k.lower() == 'retry-after'), '1')
        finally:
            for connection in connections:
                connection.close()
        deadline = time.monotonic() + 3
        while True:
            status, _, _ = self.client.raw(json.dumps(body).encode(), headers)
            if status == 200 or time.monotonic() >= deadline:
                break
            time.sleep(0.02)
        self.assertEqual(status, 200)

    def test_non_loopback_binding_is_rejected(self):
        result = subprocess.run([str(BINARY), '--transport', 'http', '--listen', '0.0.0.0:0'],
                                capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'loopback', result.stderr)

    def test_body_deadline_does_not_time_out_native_work(self):
        instance = self.open()
        worker = self.server.worker()
        os.kill(worker, signal.SIGSTOP)
        client = HttpClient(self.server.url, timeout=20)
        connection = client.connection()
        pool = ThreadPoolExecutor(max_workers=1)
        pending = pool.submit(client.call, 'find_classes', {'instanceId': instance, 'query': {}})
        try:
            connection.putrequest('POST', '/mcp')
            connection.putheader('Content-Type', 'application/json')
            connection.putheader('Accept', 'application/json, text/event-stream')
            connection.putheader('Content-Length', '10000')
            connection.endheaders()
            connection.send(b'{')
            response = connection.getresponse()
            self.assertEqual(response.status, 408)
            response.read()
            self.assertFalse(pending.done(), 'Native operation inherited the body-read timeout')
        finally:
            connection.close()
            os.kill(worker, signal.SIGCONT)
            pool.shutdown(wait=True)
        self.assertEqual(pending.result()['resultSet']['totalItems'], '1')

    def test_official_sdk_client(self):
        result = subprocess.run([str(SDK_CLIENT), self.server.url, str(self.dex)],
                                capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
        self.assertIn(b'Official SDK HTTP', result.stdout)

    def test_shutdown_reaps_blocked_worker(self):
        worker = self.server.worker()
        os.kill(worker, signal.SIGSTOP)
        connection = self.client.connection()
        body, headers = self.client.envelope('tools/call', {'name': 'dexkit_v1_capabilities', 'arguments': {}})
        connection.request('POST', '/mcp', json.dumps(body).encode(), headers)
        try:
            self.server.close(signal.SIGINT)
            with self.assertRaises(ProcessLookupError):
                os.kill(worker, 0)
        finally:
            connection.close()
        self.server = HttpProcess(self.root)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, default=BINARY)
    parser.add_argument('--sdk-client', type=Path, default=SDK_CLIENT)
    parser.add_argument('--captures', type=Path, default=common.ROOT / 'mcp/target/http-contract-cases.json')
    options, rest = parser.parse_known_args()
    BINARY = options.binary.resolve()
    SDK_CLIENT = options.sdk_client.resolve()
    result = unittest.main(argv=[__file__] + rest, exit=False)
    options.captures.parent.mkdir(parents=True, exist_ok=True)
    options.captures.write_text(json.dumps({'tools': list(common.TOOLS.values()), 'cases': common.CASES}, ensure_ascii=True))
    raise SystemExit(0 if result.result.wasSuccessful() else 1)
