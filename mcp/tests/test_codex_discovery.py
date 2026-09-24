#!/usr/bin/env python3
"""Optional installed-Codex check using a local scripted model endpoint.

No real model/API call is made. This verifies client conversion and delivery,
not an autonomous model's ability to choose tools or understand the contract.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
PRELUDE = '''
const call = async (name, args) => {
  const tool = ALL_TOOLS.find(t => t.name.endsWith("dexkit_probe__dexkit_" + name));
  if (!tool) throw new Error("Missing tool " + name);
  const reply = await tools[tool.name](args);
  const result = reply.structuredContent || JSON.parse(reply.content.find(c => c.type === "text").text);
  if (!result.ok) throw new Error(JSON.stringify(result));
  return result.data;
};
'''


def code_steps(fixture):
    return [
        PRELUDE + '''
text({declarations: ALL_TOOLS.filter(t => /dexkit_probe__dexkit_(get_query_schema|find_methods)$/.test(t.name)),
      overview: await call("get_query_schema", {tool:"dexkit_find_methods"})});
''',
        PRELUDE + '''
text({fullContract: await call("get_query_schema", {tool:"dexkit_find_methods",pointer:""})});
''',
        PRELUDE + '''
const overview = await call("get_query_schema", {tool:"dexkit_find_methods"});
const opened = await call("open", {path:''' + json.dumps(str(fixture)) + '''});
try {
  const example = overview.examples[1];
  const result = await call("find_methods", {...example, instanceId:opened.instanceId});
  if(result.resultSet.totalItems !== "1") throw new Error(JSON.stringify(result));
  text({queryFromContractPassed:true, total:result.resultSet.totalItems});
} finally { await call("close", {instanceId:opened.instanceId}); }
''',
    ]


def outputs(request):
    return [item for item in request.get('input', [])
            if item.get('type') in ('custom_tool_call_output', 'function_call_output')]


def native_definitions(request):
    result = {}
    def walk(value):
        if isinstance(value, dict):
            if value.get('type') == 'function' and value.get('name', '').startswith('dexkit_'):
                result[value['name']] = value
            for child in value.values():
                walk(child)
        elif isinstance(value, list):
            for child in value:
                walk(child)
    walk(request)
    return result


def result_objects(value):
    """Decode JSON text blocks inside actual tool outputs, not model commands."""
    if isinstance(value, str):
        if value.startswith('Wall time: ') and '\nOutput:\n' in value:
            value = value.split('\nOutput:\n', 1)[1]
        try:
            yield from result_objects(json.loads(value))
        except (ValueError, TypeError):
            pass
    elif isinstance(value, dict):
        yield value
        for child in value.values():
            yield from result_objects(child)
    elif isinstance(value, list):
        for child in value:
            yield from result_objects(child)


def run(options, mode):
    captured = []
    steps = code_steps(options.fixture)
    errors = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            data = b'{"models":[]}'
            self.send_response(200)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def do_POST(self):
            request = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            index = len(captured)
            captured.append(request)
            item = {'id': 'msg_probe', 'type': 'message', 'role': 'assistant', 'status': 'completed',
                    'content': [{'type': 'output_text', 'text': 'Local contract probe complete.', 'annotations': []}]}
            if mode == 'code' and index < len(steps):
                item = {'id': f'item_{index}', 'type': 'custom_tool_call', 'call_id': f'probe_{index}',
                        'name': 'exec', 'namespace': 'functions', 'status': 'completed', 'input': steps[index]}
            elif mode == 'native' and index == 0:
                item = {'id': 'search_probe', 'type': 'tool_search_call', 'call_id': 'search_probe',
                        'execution': 'client', 'status': 'completed',
                        'arguments': {'query': 'dexkit_probe dexkit_get_query_schema dexkit_find_methods', 'limit': 3}}
            elif mode == 'native' and index == 1:
                item = {'id': 'help_probe', 'type': 'function_call', 'call_id': 'help_probe',
                        'name': 'dexkit_get_query_schema', 'namespace': 'mcp__dexkit_probe', 'status': 'completed',
                        'arguments': json.dumps({'tool': 'dexkit_find_methods', 'pointer': ''})}
            response = {'id': f'resp_{index}', 'object': 'response', 'status': 'completed', 'output': [item],
                        'usage': {'input_tokens': 0, 'output_tokens': 0, 'total_tokens': 0}}
            events = [
                {'type': 'response.created', 'response': {**response, 'status': 'in_progress', 'output': []}},
                {'type': 'response.output_item.added', 'output_index': 0, 'item': item},
                {'type': 'response.output_item.done', 'output_index': 0, 'item': item},
                {'type': 'response.completed', 'response': response},
            ]
            data = ''.join('event: ' + e['type'] + '\ndata: ' + json.dumps(e) + '\n\n' for e in events).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError) as error:
                errors.append(type(error).__name__)

    with tempfile.TemporaryDirectory(prefix='dexkit-codex-discovery-') as work:
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        configurations = [
            'model_providers.contract_probe={name="Local contract probe",base_url="http://127.0.0.1:'
            + str(server.server_port) + '/v1",wire_api="responses",requires_openai_auth=false,request_max_retries=0,stream_max_retries=0}',
            'model_provider="contract_probe"', 'approval_policy="never"',
            'features.code_mode=' + str(mode == 'code').lower(),
            'features.code_mode_host=' + str(mode == 'code').lower(),
            'mcp_servers.dexkit_probe={command=' + json.dumps(str(options.binary))
            + ',args=["--allow-root",' + json.dumps(str(options.fixture.parent)) + ']}',
        ]
        if mode == 'native':
            configurations.append('model="gpt-5.5"')
        command = [str(options.codex)]
        for configuration in configurations:
            command.extend(['-c', configuration])
        command += ['exec', '--ignore-user-config', '--ignore-rules', '--ephemeral',
                    '--skip-git-repo-check', '--json', '-C', work,
                    'Local scripted MCP contract test. Only use the supplied tools.']
        environment = {**os.environ, 'HTTP_PROXY': '', 'HTTPS_PROXY': '', 'ALL_PROXY': '',
                       'http_proxy': '', 'https_proxy': '', 'all_proxy': ''}
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, env=environment, start_new_session=True)
        try:
            stdout, stderr = process.communicate(timeout=55)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            process.communicate(timeout=5)
            raise AssertionError('Codex probe timed out')
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)
        assert process.returncode == 0, stderr.decode(errors='replace')[-1600:]
        assert not errors, errors
        assert len(captured) == (4 if mode == 'code' else 3), (mode, len(captured), stdout.decode()[-1500:])
        delivered = [item.get('output') for request in captured[1:] for item in outputs(request)]
        objects = list(result_objects(delivered))
        published = json.loads(subprocess.check_output([str(options.binary), '--dump-schema']))
        expected = next(tool['inputSchema'] for tool in published if tool['name'] == 'dexkit_find_methods')
        assert any(obj.get('kind') == 'full' and obj.get('fragment') == expected for obj in objects), (mode, 'Full schema was not delivered intact')
        assert any(any('null entries are positional wildcards' in note for note in obj.get('notes', []))
                   for obj in objects), mode
        if mode == 'code':
            assert any(obj.get('queryFromContractPassed') is True and obj.get('total') == '1' for obj in objects)
            declaration = next(obj['declarations'] for obj in objects if 'declarations' in obj)
            helper = next(tool for tool in declaration if tool['name'].endswith('get_query_schema'))
            assert 'pointer?: string' in helper['description'] and 'dexkit_find_methods' in helper['description']
            query_declaration = next(tool['description'] for tool in declaration if tool['name'].endswith('find_methods'))
            assert 'select?: Array<"descriptor" | "flags" | "source">' in query_declaration.replace('\\"', '"')
        else:
            definitions = {name: tool for request in captured for name, tool in native_definitions(request).items()}
            helper = definitions['dexkit_get_query_schema']['parameters']
            assert 'dexkit_find_methods' in helper['properties']['tool']['enum']
            assert helper['properties']['pointer']['type'] == 'string'
            query = definitions['dexkit_find_methods']['parameters']
            assert query['properties']['select']['items']['enum'] == ['descriptor', 'flags', 'source']
        return {'mode': mode, 'modelRequests': len(captured), 'contractDelivered': True,
                'queryExecuted': mode == 'code'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--codex', type=Path, required=True)
    parser.add_argument('--binary', type=Path, default=ROOT / 'mcp/target/debug/dexkit-mcp')
    parser.add_argument('--fixture', type=Path, default=ROOT / 'tests/interop/planus/target/fixture/fixture.dex')
    parser.add_argument('--output', type=Path, default=ROOT / 'mcp/target/codex-discovery.json')
    options = parser.parse_args()
    options.codex = options.codex.resolve()
    options.binary = options.binary.resolve()
    options.fixture = options.fixture.resolve()
    version = subprocess.check_output([str(options.codex), '--version'], text=True).strip()
    results = [run(options, mode) for mode in ['code', 'native']]
    report = {'clientVersion': version, 'scriptedModel': True, 'realModelInvoked': False, 'results': results}
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
