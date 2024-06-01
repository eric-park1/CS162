use tokio::io::AsyncWriteExt;
use std::env;
use tokio::fs;
use std::net::{Ipv4Addr, SocketAddrV4};
use std::error::Error;

use crate::args;

use crate::http::*;
use crate::stats::*;

use clap::Parser;
use tokio::net::{TcpListener, TcpStream};

use anyhow::Result;

pub fn main() -> Result<()> {
    // Configure logging
    // You can print logs (to stderr) using
    // `log::info!`, `log::warn!`, `log::error!`, etc.
    env_logger::Builder::new()
        .filter_level(log::LevelFilter::Info)
        .init();

    // Parse command line arguments
    let args = args::Args::parse();

    // Set the current working directory
    env::set_current_dir(&args.files)?;

    // Print some info for debugging
    log::info!("HTTP server initializing ---------");
    log::info!("Port:\t\t{}", args.port);
    log::info!("Num threads:\t{}", args.num_threads);
    log::info!("Directory:\t\t{}", &args.files);
    log::info!("----------------------------------");

    // Initialize a thread pool that starts running `listen`
    tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .worker_threads(args.num_threads)
        .build()?
        .block_on(listen(args.port))
}

async fn listen(port: u16) -> Result<()> {
    // Hint: you should call `handle_socket` in this function.
    let addr = SocketAddrV4::new(Ipv4Addr::new(0, 0, 0, 0), port);
    let listener = TcpListener::bind(addr).await?;
    //println!("Server running on {}:{}", host_ip, port);

    loop {
        let (socket, _) = listener.accept().await.unwrap();
        tokio::spawn(async move {
            if let Err(e) = handle_socket(socket).await {
                eprintln!("Error: {}", e);
            }
        });
    }
}

// Handles a single connection via `socket`.
async fn handle_socket(mut socket: TcpStream) -> Result<()> {
    match parse_request(&mut socket).await {
        Ok(request) => {
            let method = request.method;
            let path = request.path;

            match method.as_str() {
                "GET" => {
                    let a = format!(".{}", &path);
                    if !is_valid_path(&a).await {
                        handle_not_found(&mut socket).await?
                    }
                    match std::fs::metadata(&a) {
                        Ok(metadata) => {
                            if metadata.is_file() {
                                handle_request_file(&mut socket, &path).await?;
                            } else if metadata.is_dir() {
                                let html_path = format_index(&path);
                                let b = format!(".{}", &html_path);
                                if is_valid_path(&b).await {    
                                    handle_request_file(&mut socket, &html_path).await?;
                                } else { 
                                    //let c = format!(".{}", &path);
                                    handle_dir_children(&mut socket, &a).await?;
                                }
                                    //handle_not_found(&mut socket).await?
                            } else {
                                handle_not_found(&mut socket).await?;
                            }
                        }
                        Err(e) => {
                            handle_not_found(&mut socket).await?;
                        }
                    }
                }
                _ => { handle_not_found(&mut socket).await?; }
            }
        }
        Err(e) => {
            log::warn!("Error parsing request: {}", e);
            handle_not_found(&mut socket).await?;
        }
    }

    Ok(())
}

async fn is_valid_path(path: &str) -> bool {
    std::path::Path::new(path).exists()
}

async fn is_directory(path: &str) -> bool {
    let a = format!(".{}", &path);
    if let Ok(metadata) = tokio::fs::metadata(&a).await {
        metadata.is_dir()
    } else {
        false
    }
}

// You are free (and encouraged) to add other funtions to this file.
// You can also create your own modules as you see fit.

async fn handle_dir_children(stream: &mut TcpStream, dir_path: &str) -> Result<()> {

    if !is_valid_path(&dir_path).await {
        handle_not_found(stream).await?
    }

    let mut dir_list = tokio::fs::read_dir(dir_path).await?;
    let mut structure = String::new();

    while let Some(c) = dir_list.next_entry().await? {
        //let full_path = std::path::Path::new(&dir_path);
        let href_file_name = c.file_name().to_string_lossy().to_string();
        let full_path_str = format!("{}/{}/", &dir_path, &href_file_name);
        //println!("{}", &full_path_str);
        let file_name = format!("./{}", &href_file_name);
        let href = format_href(&full_path_str, &file_name);
        structure += &href;
    }

    let h = std::path::Path::new(dir_path);
    let g = h.parent().expect("REASON").to_str().unwrap();
    let h = format_href(&g, &g);
    structure += &h;
    println!("{}", &h);


    let mime_type = "text/html";
    //get_mime_type(dir_path);
    let len = structure.len();

    start_response(stream, 200).await?;
    send_header(stream, "Content-Type", mime_type).await?;
    send_header(stream, "Content-Length", &len.to_string()).await?;
    end_headers(stream).await?;
    tokio::io::AsyncWriteExt::write_all(stream, structure.as_bytes()).await?;

    Ok(())
}


async fn handle_request_file(stream: &mut TcpStream, path: &str) -> Result<()> {

    let file_path = format!(".{}", path);

    let mut f = tokio::fs::File::open(&file_path).await?;
    let metadata = f.metadata().await?;

    let file = match tokio::fs::read(&file_path).await {
        Ok(file) => {
            let file_path = format!(".{}", path);
            let mime_type = get_mime_type(path); //header mime
            //let len = file.len();                //header content length
        
            start_response(stream, 200).await?;
            send_header(stream, "Content-Type", mime_type).await?;
            send_header(stream, "Content-Length", &metadata.len().to_string()).await?;
            end_headers(stream).await?;
            let mut buf = vec![0; 1024];
            let mut file = tokio::fs::File::open(&file_path).await?;
            loop {
                let bytes_read = tokio::io::AsyncReadExt::read(&mut file, &mut buf).await?;
                if bytes_read == 0 {
                    break;
                } else {
                    //println!("MMMM");
                    tokio::io::AsyncWriteExt::write_all(stream, &buf[..bytes_read]).await?;
                }   
            }
        }
        Err(e) => {
            handle_not_found(stream).await?;
        }
    };

    Ok(())
}

async fn handle_not_found(stream: &mut TcpStream) -> Result<()> {
    start_response(stream, 404).await;
    end_headers(stream).await;
    Ok(())
}

// async fn handle_none(stream: &mut TcpStream) -> Result<()> {
//     start_response(stream, 404).await;
//     end_headers(stream).await?;
//     Ok(())
// }