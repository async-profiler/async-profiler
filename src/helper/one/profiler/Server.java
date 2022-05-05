/*
 * Copyright 2022 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package one.profiler;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicInteger;

class Server extends Thread implements Executor, HttpHandler {
    private static final String[] COMMANDS = "start,resume,stop,dump,check,status,list,version".split(",");

    private final HttpServer server;
    private final AtomicInteger threadNum = new AtomicInteger();

    private Server(String address) throws IOException {
        super("Async-profiler Server");
        setDaemon(true);

        int p = address.lastIndexOf(':');
        InetSocketAddress socketAddress = p >= 0
                ? new InetSocketAddress(address.substring(0, p), Integer.parseInt(address.substring(p + 1)))
                : new InetSocketAddress(Integer.parseInt(address));

        server = HttpServer.create(socketAddress, 0);
        server.createContext("/", this);
        server.setExecutor(this);
    }

    public static void start(String address) throws IOException {
        new Server(address).start();
    }

    @Override
    public void run() {
        server.start();
    }

    @Override
    public void execute(Runnable command) {
        Thread t = new Thread(command, "Async-profiler Request #" + threadNum.incrementAndGet());
        t.setDaemon(false);
        t.start();
    }

    @Override
    public void handle(HttpExchange exchange) throws IOException {
        try {
            String command = getCommand(exchange.getRequestURI());
            if (command == null) {
                sendResponse(exchange, 404, "Unknown command");
            } else {
                String response = execute0(command);
                sendResponse(exchange, 200, response);
            }
        } catch (IllegalArgumentException e) {
            sendResponse(exchange, 400, e.getMessage());
        } catch (Exception e) {
            sendResponse(exchange, 500, e.getMessage());
        } finally {
            exchange.close();
        }
    }

    private String getCommand(URI uri) {
        String path = uri.getPath();
        if (path.startsWith("/")) {
            if ((path = path.substring(1)).isEmpty()) {
                return "version=full";
            }
            for (String command : COMMANDS) {
                if (path.startsWith(command)) {
                    String query = uri.getQuery();
                    return query == null ? path : path + ',' + query.replace('&', ',');
                }
            }
        }
        return null;
    }

    private void sendResponse(HttpExchange exchange, int code, String body) throws IOException {
        String contentType = body.startsWith("<!DOCTYPE html>") ? "text/html; charset=utf-8" : "text/plain";
        exchange.getResponseHeaders().add("Content-Type", contentType);

        byte[] bodyBytes = body.getBytes(StandardCharsets.UTF_8);
        exchange.sendResponseHeaders(code, bodyBytes.length);
        exchange.getResponseBody().write(bodyBytes);
    }

    private native String execute0(String command) throws IllegalArgumentException, IllegalStateException, IOException;
}
